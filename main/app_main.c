//////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                       _              //
//               _    _       _      _        _     _   _   _    _   _   _        _   _  _   _          //
//           |  | |  |_| |\| |_| |\ |_|   |\ |_|   |_| |_| | |  |   |_| |_| |\/| |_| |  |_| | |   /|    //    
//         |_|  |_|  |\  | | | | |/ | |   |/ | |   |   |\  |_|  |_| |\  | | |  | | | |_ | | |_|   _|_   //
//                                                                                       /              //
//////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
*   Programa básico para controle da placa durante a Jornada da Programação 1
*   Permite o controle das entradas e saídas digitais, entradas analógicas, display LCD e teclado. 
*   Cada biblioteca pode ser consultada na respectiva pasta em componentes
*   Existem algumas imagens e outros documentos na pasta Recursos
*   O código principal pode ser escrito a partir da linha 86
*/

// Área de inclusão das bibliotecas
//-----------------------------------------------------------------------------------------------------------------------
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "HCF_IOTEC.h"
#include "HCF_LCD.h"
#include "HCF_ADC.h"
#include "HCF_MP.h"
#include "driver/adc.h"
#include "driver/gpio.h"

// Definição de constantes
#define DEBOUNCE_DELAY 200      // Tempo de debounce para teclado (ms)
#define MAX_TENTATIVAS 3        // Número máximo de tentativas de senha
#define ADC_VALOR_MIN 465
#define ADC_VALOR_MAX 2160

// Declaração de variáveis globais
static const char *TAG = "Sistema";       // Tag para logs
static uint8_t entradas, saidas = 0;      // Variáveis para I/O
static char tecla = '-';                  // Tecla atual lida
static char ultima_tecla = '-';           // Última tecla lida (para debounce)
static uint32_t ultimo_tempo = 0;         // Último tempo de leitura de tecla
char senha[5] = "1111";                   // Senha cadastrada
char entrada_usuario[5] = "    ";         // Senha digitada pelo usuário
int posicao_digito = 0;                   // Posição atual na senha digitada
int acessos = 0;                          // Contador de acessos bem-sucedidos
int tentativas_erradas = 0;               // Contador de tentativas inválidas
bool sistema_bloqueado = false;           // Flag de bloqueio do sistema
char escrever[40];                        // Buffer para mensagens no LCD

bool senha_correta = false;               // Flag se a senha foi validada
int opcao = 0;                            // Opção escolhida pelo usuário (abrir/fechar)
char grau_digito[4] = "";                 // Digitação de graus
int pos_grau = 0;                         // Posição atual na digitação de graus
int valor_adc = 0;

// Função para limpar senha digitada e resetar o display
void limpar_digitacao() {
    memset(entrada_usuario, ' ', 5);              // Preenche com espaços
    posicao_digito = 0;                           // Reseta posição
    escreve_lcd(1, 0, "Digite a senha: ");        // Mensagem no LCD
    sprintf(escrever, "Acessos: %d", acessos);    // Mostra número de acessos
    escreve_lcd(2, 0, escrever);                  // Exibe no LCD
}

// Função para bloquear o sistema após tentativas inválidas
void bloquear_sistema() {
    sistema_bloqueado = true;                     // Seta flag de bloqueio
    limpar_lcd();                                 // Limpa o display
    escreve_lcd(1, 0, " SISTEMA");                // Mensagem de bloqueio
    escreve_lcd(2, 0, " BLOQUEADO");
}

void config_adc() {
    adc1_config_width(ADC_WIDTH_BIT_12);                  // Resolução de 12 bits
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_12); // Atenuação de 11dB
}

// Função principal do programa (executada pelo FreeRTOS)
void app_main(void) {
    escrever[39] = '\0';                             // Garante fim de string

    // Mensagens de log via terminal
    ESP_LOGI(TAG, "Iniciando sistema...");
    ESP_LOGI(TAG, "Versão do IDF: %s", esp_get_idf_version());

    // Inicialização dos periféricos customizados
    iniciar_iotec();
    entradas = io_le_escreve(saidas);
    iniciar_lcd();
    config_adc();
    iniciar_MP(6,7);

    // Exibe mensagem inicial no display
    escreve_lcd(1, 0, "    Jornada 1   ");
    escreve_lcd(2, 0, " Programa Basico");

    vTaskDelay(1000 / portTICK_PERIOD_MS);    // Espera 1 segundo
    limpar_lcd();                             // Limpa display
    limpar_digitacao();                       // Prepara tela para digitação

    // Loop principal do sistema
    while (1) {
        valor_adc = adc1_get_raw(ADC1_CHANNEL_0);
        tecla = le_teclado();                 // Lê tecla do teclado matricial

        if (sistema_bloqueado) {              // Se bloqueado, espera e ignora input
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        // Armazena tempo atual
        uint32_t tempo_atual = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // Verifica se tecla 'C' foi pressionada (reset)
        if (tecla == 'C' && (tecla != ultima_tecla || (tempo_atual - ultimo_tempo) > DEBOUNCE_DELAY)) {
            ultima_tecla = tecla;
            ultimo_tempo = tempo_atual;

            // Reseta estado conforme senha validada ou não
            if (!senha_correta) {
                limpar_digitacao();
            } else {
                senha_correta = false;
                opcao = 0;
                pos_grau = 0;
                limpar_digitacao();
            }
            vTaskDelay(50 / portTICK_PERIOD_MS);
            continue;
        }

        // Se senha ainda não foi validada
        if (!senha_correta) {
            // Se digitou número válido e não ultrapassou 4 dígitos
            if (tecla >= '0' && tecla <= '9' && posicao_digito < 4 &&
                (tecla != ultima_tecla || (tempo_atual - ultimo_tempo) > DEBOUNCE_DELAY)) {

                entrada_usuario[posicao_digito] = tecla;
                posicao_digito++;

                ultima_tecla = tecla;
                ultimo_tempo = tempo_atual;

                // Mostra '*' no lugar dos números
                escreve_lcd(1, 0, "                ");
                for (int i = 0; i < posicao_digito; i++) {
                    escrever[i] = '*';
                }
                escrever[posicao_digito] = '\0';
                escreve_lcd(1, 0, escrever);

                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
            // Verifica se pressionou '=' para confirmar senha
            else if (tecla == '=' && (tecla != ultima_tecla || (tempo_atual - ultimo_tempo) > DEBOUNCE_DELAY)) {
                ultima_tecla = tecla;
                ultimo_tempo = tempo_atual;

                // Compara senha digitada com a cadastrada
                if (posicao_digito == 4 && strncmp(entrada_usuario, senha, 4) == 0) {
                    acessos++;
                    tentativas_erradas = 0;
                    senha_correta = true;

                    // Mostra menu de opções
                    escreve_lcd(1, 0, "1-Abrir  2-Fechar");
                    escreve_lcd(2, 0, "Escolha: ");
                } else {
                    tentativas_erradas++;
                    int tentativas_restantes = MAX_TENTATIVAS - tentativas_erradas;

                    escreve_lcd(1, 0, "Senha Incorreta!");
                    sprintf(escrever, "Tentativas: %d/3", tentativas_restantes);
                    escreve_lcd(2, 0, escrever);

                    // Se estourar tentativas, bloqueia sistema
                    if (tentativas_erradas >= MAX_TENTATIVAS) {
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                        bloquear_sistema();
                        continue;
                    }
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                    limpar_digitacao();
                }
            }
        }

        // Se senha correta e sem opção selecionada
        else if (senha_correta && opcao == 0) {
            if ((tecla == '1' || tecla == '2') &&
                (tecla != ultima_tecla || (tempo_atual - ultimo_tempo) > DEBOUNCE_DELAY)) {
                opcao = tecla - '0';
                ultima_tecla = tecla;
                ultimo_tempo = tempo_atual;

                escreve_lcd(1, 0, "Digite os Graus:");
                escreve_lcd(2, 0, "                ");
                memset(grau_digito, 0, sizeof(grau_digito));
                pos_grau = 0;

                vTaskDelay(200 / portTICK_PERIOD_MS);
            }
        }

        // Se senha correta e opção já escolhida (digitando graus)
        else if (senha_correta && opcao != 0) {
            if (tecla >= '0' && tecla <= '9' && pos_grau < 3 &&
                (tecla != ultima_tecla || (tempo_atual - ultimo_tempo) > DEBOUNCE_DELAY)) {
                grau_digito[pos_grau++] = tecla;
                grau_digito[pos_grau] = '\0';

                escreve_lcd(2, 0, grau_digito);

                ultima_tecla = tecla;
                ultimo_tempo = tempo_atual;
                vTaskDelay(50 / portTICK_PERIOD_MS);
            }
            // Confirmou ângulo com '='
            else if (tecla == '=' && pos_grau > 0 &&
                     (tecla != ultima_tecla || (tempo_atual - ultimo_tempo) > DEBOUNCE_DELAY)) {
                ultima_tecla = tecla;
                ultimo_tempo = tempo_atual;

                int graus = atoi(grau_digito); // Converte para inteiro

                // Se grau maior que 90, pede confirmação
                if (graus > 90) {
                    escreve_lcd(1, 0, " RISCO ABERTURA!");
                    escreve_lcd(2, 0, "1= Ok  2=Cancel");
                    vTaskDelay(1000 / portTICK_PERIOD_MS);

                    char confirma = '-';
                    while (confirma != '1' && confirma != '2') {
                        confirma = le_teclado();
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                    }

                    if (confirma == '2') {
                        escreve_lcd(1, 0, "Operacao Cancel");
                        escreve_lcd(2, 0, "Retornando...");
                        vTaskDelay(1500 / portTICK_PERIOD_MS);
                        senha_correta = false;
                        opcao = 0;
                        pos_grau = 0;
                        limpar_digitacao();
                        continue;
                    }
                }

                escreve_lcd(1, 0, "Movendo Motor...");
                escreve_lcd(2, 0, "                ");
                vTaskDelay(500 / portTICK_PERIOD_MS);

                // Verifica valor do ADC antes de mover o motor
                if (valor_adc < ADC_VALOR_MAX && valor_adc > ADC_VALOR_MIN) {
                    rotacionar_MP(opcao == 1 ? 1 : 0, graus, saidas); // Move motor usando NPN
                } else {
                    escreve_lcd(1, 0, "Condicoes invalidas");
                    escreve_lcd(2, 0, "para mover motor");
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                }
                
                vTaskDelay(2000 / portTICK_PERIOD_MS);

                senha_correta = false;
                opcao = 0;
                pos_grau = 0;
                limpar_digitacao();
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS); // Delay no loop principal
    }
}