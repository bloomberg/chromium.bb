/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/*!\file
 * \brief Describes film grain parameters and film grain synthesis
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "aom_dsp/grain_synthesis.h"
#include "aom_mem/aom_mem.h"

// Samples with Gaussian distribution in the range of [-2048, 2047] (12 bits)
// with zero mean and standard deviation of about 512.
// should be divided by 4 for 10-bit range and 16 for 8-bit range.
static const int gaussian_sequence[2048] = {
  711,   -320,  81,    420,   447,   1637,  -197,  410,   128,   952,   -937,
  -250,  94,    320,   32,    1021,  -437,  214,   452,   -306,  515,   262,
  -81,   169,   371,   748,   65,    326,   250,   -398,  1012,  -513,  -316,
  -803,  -1051, 885,   -373,  -163,  900,   85,    433,   -725,  -61,   -322,
  179,   96,    130,   -628,  432,   1103,  345,   -143,  -346,  373,   159,
  458,   -511,  -722,  -604,  911,   -588,  924,   -323,  -442,  19,    -32,
  211,   437,   417,   305,   767,   246,   806,   492,   52,    -488,  -606,
  655,   -78,   119,   129,   -711,  721,   191,   307,   548,   100,   -469,
  -229,  583,   -66,   12,    149,   -119,  619,   -488,  -739,  -425,  -786,
  -95,   -597,  -90,   -8,    -49,   568,   -954,  252,   -177,  605,   -117,
  780,   552,   -211,  288,   375,   -371,  -705,  -130,  307,   45,    21,
  -507,  -551,  169,   375,   321,   45,    -332,  634,   348,   -598,  26,
  369,   451,   -739,  -814,  -410,  -222,  286,   746,   51,    1308,  -196,
  -605,  -468,  -31,   691,   277,   92,    -34,   -770,  -938,  451,   -218,
  -728,  1164,  -709,  -1613, -156,  800,   89,    367,   -153,  -409,  -147,
  -116,  152,   -575,  317,   715,   -510,  -794,  -590,  140,   -477,  -625,
  -146,  550,   443,   397,   -1320, 340,   -172,  -173,  -6,    269,   -85,
  161,   420,   -393,  351,   43,    152,   -757,  -60,   -480,  598,   208,
  -154,  129,   -555,  305,   -45,   398,   89,    222,   -237,  -375,  1076,
  -556,  -1171, -376,  495,   -537,  287,   879,   -365,  -815,  579,   -138,
  164,   -188,  811,   309,   287,   -300,  -241,  -189,  -1026, -1603, -1364,
  151,   -370,  -344,  -974,  -277,  982,   296,   -428,  -638,  262,   -3,
  -1445, -31,   533,   882,   -410,  189,   -321,  -390,  -814,  -597,  340,
  957,   123,   29,    531,   -689,  407,   -483,  -371,  783,   68,    593,
  540,   -472,  311,   -31,   595,   566,   -243,  524,   -212,  -617,  287,
  -63,   -461,  -593,  -434,  1218,  -310,  994,   691,   228,   -641,  -501,
  41,    74,    205,   -657,  -67,   -324,  -4,    -946,  850,   -1065, 370,
  135,   -853,  -259,  -82,   23,    -491,  521,   537,   -149,  -359,  446,
  447,   468,   -1017, -322,  -195,  250,   152,   -370,  198,   541,   293,
  -721,  -140,  -585,  -335,  -4,    414,   -793,  -563,  -420,  367,   536,
  -464,  491,   802,   -227,  575,   -379,  36,    668,   -444,  126,   146,
  348,   171,   -522,  -915,  -52,   70,    104,   -332,  605,   470,   -764,
  232,   316,   72,    400,   1697,  600,   529,   -5,    100,   119,   -491,
  -414,  1119,  -221,  658,   329,   -280,  -210,  329,   492,   397,   233,
  -49,   -590,  38,    365,   669,   1199,  -35,   44,    357,   304,   -404,
  -3,    -102,  111,   -204,  111,   -736,  1148,  -619,  -314,  148,   579,
  34,    -566,  450,   -526,  208,   -794,  -30,   -327,  -180,  164,   170,
  181,   349,   64,    -101,  -1142, 154,   33,    1127,  -104,  333,   390,
  941,   474,   -84,   105,   1207,  478,   575,   -414,  1554,  -298,  1077,
  947,   -458,  312,   -258,  -701,  -27,   934,   329,   1047,  -345,  -870,
  353,   506,   -507,  140,   160,   -243,  -717,  -299,  564,   -291,  -70,
  -446,  12,    -335,  173,   -365,  -895,  273,   354,   572,   401,   -90,
  -333,  513,   -418,  -225,  150,   16,    832,   -856,  19,    -200,  -818,
  423,   545,   -320,  246,   -159,  1095,  -24,   474,   500,   442,   -751,
  250,   31,    390,   1117,  -308,  560,   -190,  -228,  -1230, -79,   121,
  806,   -657,  694,   -999,  -17,   -191,  475,   483,   -1369, -76,   293,
  56,    -161,  -793,  630,   381,   176,   374,   368,   374,   265,   146,
  -363,  170,   8,     358,   883,   -458,  -30,   -299,  285,   -439,  -361,
  -255,  -71,   -141,  -319,  -178,  42,    -269,  -512,  42,    272,   268,
  12,    -174,  -780,  -143,  201,   108,   578,   -905,  78,    -183,  -522,
  345,   -435,  -291,  399,   888,   422,   10,    -221,  -683,  570,   -3,
  264,   163,   146,   -148,  708,   140,   269,   334,   -1045, -820,  942,
  -350,  289,   -381,  1262,  -152,  1054,  336,   246,   -107,  157,   23,
  561,   -389,  492,   55,    -318,  339,   561,   787,   -911,  309,   118,
  75,    80,    -10,   -198,  -7,    -15,   -411,  341,   386,   -120,  287,
  -33,   -22,   -753,  465,   271,   245,   517,   -414,  -526,  9,     146,
  535,   -377,  319,   281,   13,    126,   258,   112,   18,    -809,  545,
  -217,  546,   500,   -161,  139,   -741,  112,   -144,  -565,  -266,  246,
  1308,  300,   -357,  1172,  311,   -290,  136,   855,   414,   258,   165,
  138,   -56,   8,     -231,  790,   -363,  338,   618,   -366,  -86,   158,
  632,   552,   -568,  -170,  -284,  -22,   342,   -87,   669,   -230,  387,
  -1240, 222,   435,   -247,  -965,  -172,  -419,  428,   -735,  465,   32,
  -508,  16,    -428,  -161,  1091,  260,   -416,  -580,  -283,  -431,  -183,
  -952,  -853,  166,   1148,  590,   -362,  -217,  -476,  -400,  341,   -413,
  -543,  46,    247,   -184,  -94,   -475,  -503,  174,   -225,  85,    -459,
  -124,  269,   436,   445,   -958,  546,   59,    167,   226,   13,    -757,
  -158,  -289,  175,   109,   456,   -34,   -39,   45,    677,   87,    792,
  -89,   -401,  1132,  -319,  45,    143,   -344,  -1164, 539,   -72,   -399,
  301,   255,   -587,  -112,  -476,  350,   204,   650,   232,   -240,  524,
  250,   -242,  -229,  -449,  -460,  895,   -754,  -167,  286,   -904,  216,
  572,   -180,  1082,  -311,  202,   442,   674,   86,    -841,  713,   -923,
  1640,  633,   -624,  195,   603,   33,    -760,  66,    133,   -967,  -6,
  -597,  317,   -311,  577,   -410,  -657,  323,   -373,  23,    -711,  1230,
  -740,  406,   594,   -41,   -493,  -1113, -491,  488,   699,   -419,  -462,
  -203,  -133,  -346,  -407,  399,   -127,  225,   -29,   193,   -351,  650,
  -276,  352,   -577,  1060,  90,    -475,  -469,  26,    761,   401,   -447,
  -154,  1043,  377,   -21,   546,   -199,  290,   388,   -544,  1060,  -12,
  555,   -484,  12,    -185,  225,   -1053, 492,   672,   491,   -650,  -83,
  -8,    310,   -352,  477,   404,   406,   -485,  75,    733,   268,   121,
  14,    -331,  28,    234,   -282,  -150,  -558,  709,   190,   414,   66,
  -11,   -49,   -113,  130,   -196,  551,   677,   532,   -122,  57,    -165,
  167,   74,    -257,  812,   -1073, -767,  -616,  282,   219,   326,   -351,
  147,   762,   -301,  41,    1015,  29,    -383,  -21,   -1384, 110,   -785,
  103,   -52,   -151,  -584,  -850,  -328,  -571,  449,   230,   -208,  253,
  409,   73,    -262,  -141,  179,   -135,  1248,  143,   -570,  -169,  377,
  -66,   119,   335,   530,   582,   -493,  235,   391,   -1227, -1067, 780,
  -160,  409,   52,    270,   1210,  69,    58,    1390,  733,   -240,  119,
  -111,  -265,  91,    -256,  -84,   -563,  521,   -6,    28,    239,   -720,
  -855,  -407,  -661,  -682,  -814,  442,   -405,  319,   877,   -140,  -120,
  -115,  -302,  -134,  222,   -448,  -405,  -185,  -20,   193,   97,    -266,
  440,   850,   -429,  -645,  -388,  -462,  -589,  659,   -421,  -96,   57,
  212,   407,   -259,  -451,  276,   387,   455,   -42,   -624,  333,   -690,
  -102,  818,   368,   -194,  -214,  -35,   236,   -589,  270,   21,    -197,
  507,   -706,  124,   -32,   -226,  8,     301,   79,    9,     -177,  606,
  197,   -269,  -1075, -162,  1052,  499,   -51,   1312,  -1074, 52,    -141,
  351,   -261,  -898,  -513,  -253,  45,    844,   534,   199,   -176,  -490,
  -305,  894,   -229,  41,    328,   -73,   -57,   -4,    517,   -522,  126,
  -50,   -63,   478,   -787,  -377,  192,   470,   -449,  396,   -129,  233,
  411,   118,   257,   174,   508,   -542,  440,   -446,  24,    407,   570,
  -81,   -41,   14,    -696,  273,   -584,  -398,  -186,  -96,   -860,  -69,
  223,   -905,  -309,  70,    91,    433,   -1017, -228,  60,    -655,  -388,
  -357,  -500,  -1595, 486,   824,   967,   -52,   -46,   265,   -540,  -230,
  -621,  334,   -420,  -756,  848,   -191,  -1000, -780,  146,   -666,  270,
  -549,  600,   -202,  40,    834,   -586,  94,    104,   367,   112,   591,
  -204,  128,   259,   418,   456,   -257,  726,   405,   827,   -77,   89,
  833,   -372,  421,   176,   -138,  86,    -414,  302,   -457,  -1222, 431,
  -271,  117,   196,   -487,  -1082, -762,  -823,  -703,  288,   -290,  321,
  34,    -85,   563,   -327,  -7,    -1304, 892,   505,   258,   -154,  -669,
  108,   -543,  352,   273,   334,   404,   -464,  -529,  335,   -271,  -34,
  -736,  165,   -636,  -642,  -7,    -521,  -921,  -1262, -324,  533,   233,
  603,   89,    -352,  219,   379,   -182,  -180,  -43,   -60,   1208,  247,
  -215,  -484,  417,   384,   -90,   757,   -137,  224,   296,   475,   -480,
  903,   667,   -1184, -69,   -299,  502,   990,   -56,   243,   580,   -16,
  -248,  346,   -148,  723,   -423,  150,   198,   -80,   -136,  681,   -157,
  199,   198,   -37,   -214,  -356,  65,    657,   99,    -1086, 329,   733,
  200,   1099,  1514,  -315,  812,   284,   -579,  -465,  871,   -296,  983,
  797,   -36,   284,   157,   144,   940,   333,   -298,  582,   -484,  -114,
  -75,   263,   -895,  -38,   -858,  -196,  -681,  -383,  -261,  -8,    -85,
  453,   558,   -198,  454,   558,   -131,  -342,  -769,  864,   -382,  504,
  938,   603,   -437,  -123,  -741,  141,   595,   113,   -523,  -2,    32,
  -224,  -481,  -19,   -20,   -43,   244,   408,   946,   -1092, 27,    664,
  34,    328,   -768,  841,   308,   -3,    -747,  631,   -431,  -104,  551,
  -219,  372,   618,   855,   104,   -548,  54,    -251,  -529,  306,   -444,
  -927,  -300,  -25,   51,    256,   112,   -248,  -318,  68,    154,   179,
  -6,    -79,   417,   -64,   -233,  -438,  -22,   114,   -243,  -284,  -748,
  5,     794,   -345,  -373,  -317,  -173,  73,    -273,  -396,  -168,  632,
  30,    328,   -753,  205,   -69,   223,   -546,  -797,  -334,  -242,  362,
  376,   -131,  -15,   -989,  -77,   -578,  -443,  -848,  -244,  18,    141,
  -378,  699,   -907,  -198,  103,   1029,  575,   25,    -440,  662,   -631,
  917,   164,   378,   -21,   -973,  68,    354,   -788,  213,   -592,  495,
  349,   608,   180,   312,   315,   848,   -78,   -24,   -311,  95,    -141,
  -795,  561,   -383,  1456,  -136,  -228,  -578,  150,   -840,  -149,  -286,
  25,    738,   608,   762,   221,   -586,  -44,   102,   -47,   280,   -152,
  -164,  -147,  360,   1001,  348,   391,   233,   -605,  283,   -52,   -139,
  -640,  -594,  816,   -401,  835,   -81,   -152,  223,   -178,  -46,   -166,
  -16,   266,   434,   127,   99,    -468,  472,   -6,    412,   9,     100,
  489,   -852,  -1052, -277,  1017,  353,   -259,  -537,  568,   45,    -152,
  -188,  713,   860,   -60,   -767,  -41,   -490,  689,   -933,  689,   -67,
  -751,  -276,  -411,  842,   -472,  -556,  178,   -517,  228,   -474,  348,
  74,    982,   299,   -590,  805,   518,   303,   -548,  -261,  743,   1179,
  480,   286,   280,   474,   -53,   478,   -161,  339,   -44,   374,   17,
  -800,  -122,  287,   -825,  -272,  196,   -19,   -348,  -49,   -499,  273,
  -224,  -11,   -846,  485,   1,     86,    -1027, 203,   -605,  -1159, -42,
  171,   520,   -75,   84,    -759,  -519,  -473,  -650,  348,   -228,  -68,
  592,   330,   -168,  -606,  318,   146,   -255,  -688,  -500,  -540,  823,
  -250,  -703,  632,   177,   -315,  -212,  97,    -160,  107,   -640,  449,
  -72,   -875,  511,   174,   207,   901,   678,   -889,  -124,  -295,  -1132,
  -911,  -251,  -370,  537,   1298,  -165,  326,   518,   -157,  333,   354,
  -523,  -122,  -318,  22,    -200,  -402,  135,   316,   11,    317,   -570,
  -288,  535,   575,   -416,  189,   -127,  -218,  466,   414,   328,   257,
  -665,  -1015, -275,  1230,  -493,  -1791, -883,  362,   78,    300,   -31,
  -885,  -149,  410,   -727,  -387,  -890,  -304,  -44,   -441,  699,   -110,
  251,   960,   686,   367,   60,    -508,  331,   382,   138,   -151,  -745,
  32,    618,   -131,  462,   -244,  383,   996,   -493,  -150,  -594,  500,
  -363,  102,   662,   137,   -613,  512,   402,   967,   936,   371,   666,
  343,   -171,  56,    -224,  -719,  731,   -874,  47,    -254,  639,   324,
  -176,  191,   -376,  -295,  678,   703,   113,   -386,  -461,  285,   -147,
  -990,  -701,  293,   -675,  -576,  298,   -838,  713,   -489,  -386,  617,
  818,   548,   -281,  59,    201,   253,   657,   -537,  -554,  -224,  -489,
  -854,  -56,   -261,  660,   312,   282,   -778,  -73,   680,   13,    -37,
  -202,  999,   -498,  215,   -194,  -334,  -201,  626,   -823,  -339,  639,
  -355,  655,   -980,  -614,  781,   -319,  -439,  -25,   -7,    -383,  522,
  228,   153,   766,   913,   -130,  717,   538,   -489,  353,   411,   239,
  509,   -420,  -850,  883,   461,   928,   368,   -702,  -1114, -35,   112,
  -153,  642,   55,    -494,  -621,  -774,  -148,  818,   -107,  -454,  -207,
  88,    -569,  385,   793,   215,   549,   -320,  936,   -312,  -690,  973,
  -562,  -411,  675,   250,   153,   90,    -372,  547,   -1029, 503,   -60,
  263,   -322,  401,   621,   388,   511,   296,   173,   -78,   -416,  98,
  -69,   -368,  801,   -160,  871,   248,   44,    617,   1098,  175,   312,
  -750,  -149,  866,   -151,  406,   -428,  221,   -214,  -287,  -822,  262,
  -783,  682,   -179,  752,   445,   -197,  -181,  -825,  -1293, 213,   -142,
  211,   -340,  -606,  892,   -567,  -235,  781,   -703,  -276,  739,   565,
  -262,  433,   -616,  -530,  427,   -532,  931,   -49,   -81,   -1357, -402,
  530,   303,   59,    876,   -377,  -998,  339,   -680,  -49,   -157,  -213,
  -850,  507,   -290,  196,   622,   -523,  268,   370,   -132,  -749,  91,
  -558,  5,     -240,  -55,   -8,    -447,  -291,  552,   -238,  283,   -93,
  224,   873,   263,   -416,  229,   -229,  202,   -113,  839,   396,   -143,
  -184,  564,   186,   240,   -96,   -791,  225,   -68,   532,   9,     -441,
  670,   156,   703,   -322,  -1190, -362,  -1010, -633,  -265,  -484,  20,
  -369,  -325,  -379,  -286,  -67,   -122,  449,   -845,  81,    436,   116,
  -521,  -59,   -1379, 400,   852,   -471,  -880,  638,   -875,  155,   -375,
  -183,  1032,  63,    -266,  -272,  298,   -186,  504,   273,   235,   116,
  364,   671,   -619,  -174,  -740,  -535,  394,   124,   -877,  -288,  -146,
  37,    641,   -183,  498,   142,   -109,  -92,   540,   -587,  -170,  -469,
  -1347, 852,   -313,  176,   366,   220,   -403,  -384,  135,   -321,  706,
  254,   575,   511,   -6,    -363,  -497,  -611,  -457,  475,   245,   374,
  1811,  123
};

static const int gauss_bits = 11;

static const int luma_subblock_size = 32;
static const int chroma_subblock_size = 16;

static const int min_luma_legal_range = 16;
static const int max_luma_legal_range = 235;

static const int min_chroma_legal_range = 16;
static const int max_chroma_legal_range = 240;

static int scaling_lut_y[256];
static int scaling_lut_cb[256];
static int scaling_lut_cr[256];

static int grain_center;
static int grain_min;
static int grain_max;

static uint16_t random_register = 0;  // random number generator register

static void init_arrays(aom_film_grain_t *params, int luma_stride,
                        int chroma_stride, int ***pred_pos_luma_p,
                        int ***pred_pos_chroma_p, int **luma_grain_block,
                        int **cb_grain_block, int **cr_grain_block,
                        int **y_line_buf, int **cb_line_buf, int **cr_line_buf,
                        int **y_col_buf, int **cb_col_buf, int **cr_col_buf,
                        int luma_grain_samples, int chroma_grain_samples) {
  memset(scaling_lut_y, 0, sizeof(*scaling_lut_y) * 256);
  memset(scaling_lut_cb, 0, sizeof(*scaling_lut_cb) * 256);
  memset(scaling_lut_cr, 0, sizeof(*scaling_lut_cr) * 256);

  int num_pos_luma = 2 * params->ar_coeff_lag * (params->ar_coeff_lag + 1);
  int num_pos_chroma = num_pos_luma + 1;

  int **pred_pos_luma;
  int **pred_pos_chroma;

  pred_pos_luma = (int **)aom_malloc(sizeof(*pred_pos_luma) * num_pos_luma);

  for (int row = 0; row < num_pos_luma; row++) {
    pred_pos_luma[row] = (int *)aom_malloc(sizeof(**pred_pos_luma) * 3);
  }

  pred_pos_chroma =
      (int **)aom_malloc(sizeof(*pred_pos_chroma) * num_pos_chroma);

  for (int row = 0; row < num_pos_chroma; row++) {
    pred_pos_chroma[row] = (int *)aom_malloc(sizeof(**pred_pos_chroma) * 3);
  }

  int pos_ar_index = 0;

  for (int row = -params->ar_coeff_lag; row < 0; row++) {
    for (int col = -params->ar_coeff_lag; col < params->ar_coeff_lag + 1;
         col++) {
      pred_pos_luma[pos_ar_index][0] = row;
      pred_pos_luma[pos_ar_index][1] = col;
      pred_pos_luma[pos_ar_index][2] = 0;

      pred_pos_chroma[pos_ar_index][0] = row;
      pred_pos_chroma[pos_ar_index][1] = col;
      pred_pos_chroma[pos_ar_index][2] = 0;
      ++pos_ar_index;
    }
  }

  for (int col = -params->ar_coeff_lag; col < 0; col++) {
    pred_pos_luma[pos_ar_index][0] = 0;
    pred_pos_luma[pos_ar_index][1] = col;
    pred_pos_luma[pos_ar_index][2] = 0;

    pred_pos_chroma[pos_ar_index][0] = 0;
    pred_pos_chroma[pos_ar_index][1] = col;
    pred_pos_chroma[pos_ar_index][2] = 0;

    ++pos_ar_index;
  }

  pred_pos_chroma[pos_ar_index][0] = 0;
  pred_pos_chroma[pos_ar_index][1] = 0;
  pred_pos_chroma[pos_ar_index][2] = 1;

  *pred_pos_luma_p = pred_pos_luma;
  *pred_pos_chroma_p = pred_pos_chroma;

  *y_line_buf = (int *)aom_malloc(sizeof(**y_line_buf) * luma_stride * 2);
  *cb_line_buf = (int *)aom_malloc(sizeof(**cb_line_buf) * chroma_stride);
  *cr_line_buf = (int *)aom_malloc(sizeof(**cr_line_buf) * chroma_stride);

  *y_col_buf =
      (int *)aom_malloc(sizeof(**y_col_buf) * (luma_subblock_size + 2) * 2);
  *cb_col_buf =
      (int *)aom_malloc(sizeof(**cb_col_buf) * (chroma_subblock_size + 1));
  *cr_col_buf =
      (int *)aom_malloc(sizeof(**cr_col_buf) * (chroma_subblock_size + 1));

  *luma_grain_block =
      (int *)aom_malloc(sizeof(**luma_grain_block) * luma_grain_samples);
  *cb_grain_block =
      (int *)aom_malloc(sizeof(**cb_grain_block) * chroma_grain_samples);
  *cr_grain_block =
      (int *)aom_malloc(sizeof(**cr_grain_block) * chroma_grain_samples);
}

static void dealloc_arrays(aom_film_grain_t *params, int ***pred_pos_luma,
                           int ***pred_pos_chroma, int **luma_grain_block,
                           int **cb_grain_block, int **cr_grain_block,
                           int **y_line_buf, int **cb_line_buf,
                           int **cr_line_buf, int **y_col_buf, int **cb_col_buf,
                           int **cr_col_buf) {
  int num_pos_luma = 2 * params->ar_coeff_lag * (params->ar_coeff_lag + 1);
  int num_pos_chroma = num_pos_luma + 1;

  for (int row = 0; row < num_pos_luma; row++) {
    aom_free((*pred_pos_luma)[row]);
  }
  aom_free(*pred_pos_luma);

  for (int row = 0; row < num_pos_chroma; row++) {
    aom_free((*pred_pos_chroma)[row]);
  }
  aom_free((*pred_pos_chroma));

  aom_free(*y_line_buf);

  aom_free(*cb_line_buf);

  aom_free(*cr_line_buf);

  aom_free(*y_col_buf);

  aom_free(*cb_col_buf);

  aom_free(*cr_col_buf);

  aom_free(*luma_grain_block);

  aom_free(*cb_grain_block);

  aom_free(*cr_grain_block);
}

// get a number between 0 and 2^bits - 1
static INLINE int get_random_number(int bits) {
  uint16_t bit;
  bit = ((random_register >> 0) ^ (random_register >> 1) ^
         (random_register >> 3) ^ (random_register >> 12)) &
        1;
  random_register = (random_register >> 1) | (bit << 15);
  return (random_register >> (16 - bits)) & ((1 << bits) - 1);
}

static void init_random_generator(int luma_line, uint16_t seed) {
  // same for the picture

  uint16_t msb = (seed >> 8) & 255;
  uint16_t lsb = seed & 255;

  random_register = (msb << 8) + lsb;

  //  changes for each row
  int luma_num = luma_line >> 5;

  random_register ^= ((luma_num * 37 + 178) & 255) << 8;
  random_register ^= ((luma_num * 173 + 105) & 255);
}

static void generate_luma_grain_block(
    aom_film_grain_t *params, int **pred_pos_luma, int *luma_grain_block,
    int luma_block_size_y, int luma_block_size_x, int luma_grain_stride,
    int left_pad, int top_pad, int right_pad, int bottom_pad) {
  int bit_depth = params->bit_depth;

  int num_pos_luma = 2 * params->ar_coeff_lag * (params->ar_coeff_lag + 1);
  int rounding_offset = (1 << (params->ar_coeff_shift - 1));

  for (int i = 0; i < luma_block_size_y; i++)
    for (int j = 0; j < luma_block_size_x; j++)
      luma_grain_block[i * luma_grain_stride + j] =
          (gaussian_sequence[get_random_number(gauss_bits)] +
           ((1 << (12 - bit_depth)) >> 1)) >>
          (12 - bit_depth);

  for (int i = top_pad; i < luma_block_size_y - bottom_pad; i++)
    for (int j = left_pad; j < luma_block_size_x - right_pad; j++) {
      int wsum = 0;
      for (int pos = 0; pos < num_pos_luma; pos++) {
        wsum = wsum + params->ar_coeffs_y[pos] *
                          luma_grain_block[(i + pred_pos_luma[pos][0]) *
                                               luma_grain_stride +
                                           j + pred_pos_luma[pos][1]];
      }
      luma_grain_block[i * luma_grain_stride + j] =
          clamp(luma_grain_block[i * luma_grain_stride + j] +
                    ((wsum + rounding_offset) >> params->ar_coeff_shift),
                grain_min, grain_max);
    }
}

static void generate_chroma_grain_blocks(
    aom_film_grain_t *params,
    //                                  int** pred_pos_luma,
    int **pred_pos_chroma, int *luma_grain_block, int *cb_grain_block,
    int *cr_grain_block, int luma_grain_stride, int chroma_block_size_y,
    int chroma_block_size_x, int chroma_grain_stride, int left_pad, int top_pad,
    int right_pad, int bottom_pad) {
  int bit_depth = params->bit_depth;

  int num_pos_chroma =
      2 * params->ar_coeff_lag * (params->ar_coeff_lag + 1) + 1;
  int rounding_offset = (1 << (params->ar_coeff_shift - 1));

  for (int i = 0; i < chroma_block_size_y; i++)
    for (int j = 0; j < chroma_block_size_x; j++) {
      cb_grain_block[i * chroma_grain_stride + j] =
          (gaussian_sequence[get_random_number(gauss_bits)] +
           ((1 << (12 - bit_depth)) >> 1)) >>
          (12 - bit_depth);
      cr_grain_block[i * chroma_grain_stride + j] =
          (gaussian_sequence[get_random_number(gauss_bits)] +
           ((1 << (12 - bit_depth)) >> 1)) >>
          (12 - bit_depth);
    }

  for (int i = top_pad; i < chroma_block_size_y - bottom_pad; i++)
    for (int j = left_pad; j < chroma_block_size_x - right_pad; j++) {
      int wsum_cb = 0;
      int wsum_cr = 0;
      for (int pos = 0; pos < num_pos_chroma; pos++) {
        if (pred_pos_chroma[pos][2] == 0) {
          wsum_cb = wsum_cb + params->ar_coeffs_cb[pos] *
                                  cb_grain_block[(i + pred_pos_chroma[pos][0]) *
                                                     chroma_grain_stride +
                                                 j + pred_pos_chroma[pos][1]];
          wsum_cr = wsum_cr + params->ar_coeffs_cr[pos] *
                                  cr_grain_block[(i + pred_pos_chroma[pos][0]) *
                                                     chroma_grain_stride +
                                                 j + pred_pos_chroma[pos][1]];
        } else if (pred_pos_chroma[pos][2] == 1) {
          int av_luma =
              (luma_grain_block[(((i - top_pad) << 1) + top_pad) *
                                    luma_grain_stride +
                                ((j - left_pad) << 1) + left_pad] +
               luma_grain_block[(((i - top_pad) << 1) + top_pad + 1) *
                                    luma_grain_stride +
                                ((j - left_pad) << 1) + left_pad] +
               luma_grain_block[(((i - top_pad) << 1) + top_pad) *
                                    luma_grain_stride +
                                ((j - left_pad) << 1) + left_pad + 1] +
               luma_grain_block[(((i - top_pad) << 1) + top_pad + 1) *
                                    luma_grain_stride +
                                ((j - left_pad) << 1) + left_pad + 1] +
               2) >>
              2;

          wsum_cb = wsum_cb + params->ar_coeffs_cb[pos] * av_luma;
          wsum_cr = wsum_cr + params->ar_coeffs_cr[pos] * av_luma;
        } else {
          printf(
              "Grain synthesis: prediction between two chroma components is "
              "not supported!");
          exit(1);
        }
      }
      cb_grain_block[i * chroma_grain_stride + j] =
          clamp(cb_grain_block[i * chroma_grain_stride + j] +
                    ((wsum_cb + rounding_offset) >> params->ar_coeff_shift),
                grain_min, grain_max);
      cr_grain_block[i * chroma_grain_stride + j] =
          clamp(cr_grain_block[i * chroma_grain_stride + j] +
                    ((wsum_cr + rounding_offset) >> params->ar_coeff_shift),
                grain_min, grain_max);
    }
}

static void init_scaling_function(int scaling_points[][2], int num_points,
                                  int scaling_lut[]) {
  for (int i = 0; i < scaling_points[0][0]; i++)
    scaling_lut[i] = scaling_points[0][1];

  for (int point = 0; point < num_points - 1; point++) {
    int delta_y = scaling_points[point + 1][1] - scaling_points[point][1];
    int delta_x = scaling_points[point + 1][0] - scaling_points[point][0];

    int64_t delta = delta_y * ((65536 + (delta_x >> 1)) / delta_x);

    for (int x = 0; x < delta_x; x++) {
      scaling_lut[scaling_points[point][0] + x] =
          scaling_points[point][1] + (int)((x * delta + 32768) >> 16);
    }
  }

  for (int i = scaling_points[num_points - 1][0]; i < 256; i++)
    scaling_lut[i] = scaling_points[num_points - 1][1];
}

// function that extracts samples from a LUT (and interpolates intemediate
// frames for 10- and 12-bit video)
static int scale_LUT(int *scaling_lut, int index, int bit_depth) {
  int x = index >> (bit_depth - 8);

  if (!(bit_depth - 8) || x == 255)
    return scaling_lut[x];
  else
    return scaling_lut[x] + (((scaling_lut[x + 1] - scaling_lut[x]) *
                                  (index & ((1 << (bit_depth - 8)) - 1)) +
                              (1 << (bit_depth - 9))) >>
                             (bit_depth - 8));
}

static void add_noise_to_block(aom_film_grain_t *params, uint8_t *luma,
                               uint8_t *cb, uint8_t *cr, int luma_stride,
                               int chroma_stride, int *luma_grain,
                               int *cb_grain, int *cr_grain,
                               int luma_grain_stride, int chroma_grain_stride,
                               int chroma_height, int chroma_width,
                               int bit_depth) {
  int cb_mult = params->cb_mult - 128;            // fixed scale
  int cb_luma_mult = params->cb_luma_mult - 128;  // fixed scale
  int cb_offset = params->cb_offset - 256;

  int cr_mult = params->cr_mult - 128;            // fixed scale
  int cr_luma_mult = params->cr_luma_mult - 128;  // fixed scale
  int cr_offset = params->cr_offset - 256;

  int rounding_offset = (1 << (params->scaling_shift - 1));

  if (params->chroma_scaling_from_luma) {
    cb_mult = 0;        // fixed scale
    cb_luma_mult = 64;  // fixed scale
    cb_offset = 0;

    cr_mult = 0;        // fixed scale
    cr_luma_mult = 64;  // fixed scale
    cr_offset = 0;
  }

  int min_luma, max_luma, min_chroma, max_chroma;

  if (params->clip_to_restricted_range) {
    min_luma = min_luma_legal_range;
    max_luma = max_luma_legal_range;

    min_chroma = min_chroma_legal_range;
    max_chroma = max_chroma_legal_range;
  } else {
    min_luma = min_chroma = 0;
    max_luma = max_chroma = 255;
  }

  for (int i = 0; i < chroma_height; i++) {
    for (int j = 0; j < chroma_width; j++) {
      int average_luma = (luma[(i << 1) * luma_stride + (j << 1)] +
                          luma[((i << 1)) * luma_stride + (j << 1) + 1] + 1) >>
                         1;

      luma[((i) << 1) * luma_stride + ((j) << 1)] = clamp(
          luma[((i) << 1) * luma_stride + ((j) << 1)] +
              ((scale_LUT(scaling_lut_y,
                          luma[((i) << 1) * luma_stride + ((j) << 1)], 8) *
                    luma_grain[(i << 1) * luma_grain_stride + (j << 1)] +
                rounding_offset) >>
               params->scaling_shift),
          min_luma, max_luma);
      luma[(((i) << 1) + 1) * luma_stride + ((j) << 1)] = clamp(
          luma[(((i) << 1) + 1) * luma_stride + ((j) << 1)] +
              ((scale_LUT(scaling_lut_y,
                          luma[(((i) << 1) + 1) * luma_stride + ((j) << 1)],
                          8) *
                    luma_grain[((i << 1) + 1) * luma_grain_stride + (j << 1)] +
                rounding_offset) >>
               params->scaling_shift),
          min_luma, max_luma);
      luma[(((i) << 1)) * luma_stride + ((j) << 1) + 1] = clamp(
          luma[(((i) << 1)) * luma_stride + ((j) << 1) + 1] +
              ((scale_LUT(scaling_lut_y,
                          luma[(((i) << 1)) * luma_stride + ((j) << 1) + 1],
                          8) *
                    luma_grain[(i << 1) * luma_grain_stride + (j << 1) + 1] +
                rounding_offset) >>
               params->scaling_shift),
          min_luma, max_luma);
      luma[(((i) << 1) + 1) * luma_stride + ((j) << 1) + 1] = clamp(
          luma[(((i) << 1) + 1) * luma_stride + ((j) << 1) + 1] +
              ((scale_LUT(scaling_lut_y,
                          luma[(((i) << 1) + 1) * luma_stride + ((j) << 1) + 1],
                          8) *
                    luma_grain[((i << 1) + 1) * luma_grain_stride + (j << 1) +
                               1] +
                rounding_offset) >>
               params->scaling_shift),
          min_luma, max_luma);

      cb[i * chroma_stride + j] =
          clamp(cb[i * chroma_stride + j] +
                    ((scale_LUT(scaling_lut_cb,
                                clamp(((average_luma * cb_luma_mult +
                                        cb_mult * cb[i * chroma_stride + j]) >>
                                       6) +
                                          cb_offset,
                                      0, (256 << (bit_depth - 8)) - 1),
                                8) *
                          cb_grain[i * chroma_grain_stride + j] +
                      rounding_offset) >>
                     params->scaling_shift),
                min_chroma, max_chroma);

      cr[i * chroma_stride + j] =
          clamp(cr[i * chroma_stride + j] +
                    ((scale_LUT(scaling_lut_cr,
                                clamp(((average_luma * cr_luma_mult +
                                        cr_mult * cr[i * chroma_stride + j]) >>
                                       6) +
                                          cr_offset,
                                      0, (256 << (bit_depth - 8)) - 1),
                                8) *
                          cr_grain[i * chroma_grain_stride + j] +
                      rounding_offset) >>
                     params->scaling_shift),
                min_chroma, max_chroma);
    }
  }
}

static void add_noise_to_block_hbd(aom_film_grain_t *params, uint16_t *luma,
                                   uint16_t *cb, uint16_t *cr, int luma_stride,
                                   int chroma_stride, int *luma_grain,
                                   int *cb_grain, int *cr_grain,
                                   int luma_grain_stride,
                                   int chroma_grain_stride, int chroma_height,
                                   int chroma_width, int bit_depth) {
  int cb_mult = params->cb_mult - 128;            // fixed scale
  int cb_luma_mult = params->cb_luma_mult - 128;  // fixed scale
  // offset value depends on the bit depth
  int cb_offset = (params->cb_offset << (bit_depth - 8)) - (1 << bit_depth);

  int cr_mult = params->cr_mult - 128;            // fixed scale
  int cr_luma_mult = params->cr_luma_mult - 128;  // fixed scale
  // offset value depends on the bit depth
  int cr_offset = (params->cr_offset << (bit_depth - 8)) - (1 << bit_depth);

  int rounding_offset = (1 << (params->scaling_shift - 1));

  if (params->chroma_scaling_from_luma) {
    cb_mult = 0;        // fixed scale
    cb_luma_mult = 64;  // fixed scale
    cb_offset = 0;

    cr_mult = 0;        // fixed scale
    cr_luma_mult = 64;  // fixed scale
    cr_offset = 0;
  }

  int min_luma, max_luma, min_chroma, max_chroma;

  if (params->clip_to_restricted_range) {
    min_luma = min_luma_legal_range << (bit_depth - 8);
    max_luma = max_luma_legal_range << (bit_depth - 8);

    min_chroma = min_chroma_legal_range << (bit_depth - 8);
    max_chroma = max_chroma_legal_range << (bit_depth - 8);
  } else {
    min_luma = min_chroma = 0;
    max_luma = max_chroma = (256 << (bit_depth - 8)) - 1;
  }

  for (int i = 0; i < chroma_height; i++) {
    for (int j = 0; j < chroma_width; j++) {
      int average_luma = (luma[(i << 1) * luma_stride + (j << 1)] +
                          luma[((i << 1)) * luma_stride + (j << 1) + 1] + 1) >>
                         1;

      luma[((i) << 1) * luma_stride + ((j) << 1)] =
          clamp(luma[((i) << 1) * luma_stride + ((j) << 1)] +
                    ((scale_LUT(scaling_lut_y,
                                luma[((i) << 1) * luma_stride + ((j) << 1)],
                                bit_depth) *
                          luma_grain[(i << 1) * luma_grain_stride + (j << 1)] +
                      rounding_offset) >>
                     params->scaling_shift),
                min_luma, max_luma);
      luma[(((i) << 1) + 1) * luma_stride + ((j) << 1)] = clamp(
          luma[(((i) << 1) + 1) * luma_stride + ((j) << 1)] +
              ((scale_LUT(scaling_lut_y,
                          luma[(((i) << 1) + 1) * luma_stride + ((j) << 1)],
                          bit_depth) *
                    luma_grain[((i << 1) + 1) * luma_grain_stride + (j << 1)] +
                rounding_offset) >>
               params->scaling_shift),
          min_luma, max_luma);
      luma[(((i) << 1)) * luma_stride + ((j) << 1) + 1] = clamp(
          luma[(((i) << 1)) * luma_stride + ((j) << 1) + 1] +
              ((scale_LUT(scaling_lut_y,
                          luma[(((i) << 1)) * luma_stride + ((j) << 1) + 1],
                          bit_depth) *
                    luma_grain[(i << 1) * luma_grain_stride + (j << 1) + 1] +
                rounding_offset) >>
               params->scaling_shift),
          min_luma, max_luma);
      luma[(((i) << 1) + 1) * luma_stride + ((j) << 1) + 1] = clamp(
          luma[(((i) << 1) + 1) * luma_stride + ((j) << 1) + 1] +
              ((scale_LUT(scaling_lut_y,
                          luma[(((i) << 1) + 1) * luma_stride + ((j) << 1) + 1],
                          bit_depth) *
                    luma_grain[((i << 1) + 1) * luma_grain_stride + (j << 1) +
                               1] +
                rounding_offset) >>
               params->scaling_shift),
          min_luma, max_luma);

      cb[i * chroma_stride + j] =
          clamp(cb[i * chroma_stride + j] +
                    ((scale_LUT(scaling_lut_cb,
                                clamp(((average_luma * cb_luma_mult +
                                        cb_mult * cb[i * chroma_stride + j]) >>
                                       6) +
                                          cb_offset,
                                      0, (256 << (bit_depth - 8)) - 1),
                                bit_depth) *
                          cb_grain[i * chroma_grain_stride + j] +
                      rounding_offset) >>
                     params->scaling_shift),
                min_chroma, max_chroma);

      cr[i * chroma_stride + j] =
          clamp(cr[i * chroma_stride + j] +
                    ((scale_LUT(scaling_lut_cr,
                                clamp(((average_luma * cr_luma_mult +
                                        cr_mult * cr[i * chroma_stride + j]) >>
                                       6) +
                                          cr_offset,
                                      0, (256 << (bit_depth - 8)) - 1),
                                bit_depth) *
                          cr_grain[i * chroma_grain_stride + j] +
                      rounding_offset) >>
                     params->scaling_shift),
                min_chroma, max_chroma);
    }
  }
}

static void copy_rect(uint8_t *src, int src_stride, uint8_t *dst,
                      int dst_stride, int width, int height,
                      int high_bit_depth) {
  int hbd_coeff = high_bit_depth ? 2 : 0;
  while (height) {
    memcpy(dst, src, width * sizeof(uint8_t) * hbd_coeff);
    src += src_stride;
    dst += dst_stride;
    --height;
  }
  return;
}

void av1_add_film_grain(aom_film_grain_t *params, aom_image_t *src,
                        aom_image_t *dst) {
  uint8_t *luma, *cb, *cr;
  int height, width, luma_stride, chroma_stride;

  if (!(src->fmt == AOM_IMG_FMT_I42016) && !(src->fmt == AOM_IMG_FMT_I420)) {
    printf("Film grain error: only 4:2:0 is currently supported!");
    exit(1);
  }

  dst->r_w = src->r_w;
  dst->r_h = src->r_h;

  copy_rect(src->planes[AOM_PLANE_Y], src->stride[AOM_PLANE_Y],
            dst->planes[AOM_PLANE_Y], dst->stride[AOM_PLANE_Y], dst->d_w,
            dst->d_h, src->bit_depth);

  copy_rect(src->planes[AOM_PLANE_U], src->stride[AOM_PLANE_U],
            dst->planes[AOM_PLANE_U], dst->stride[AOM_PLANE_U], dst->d_w / 2,
            dst->d_h / 2, src->bit_depth);

  copy_rect(src->planes[AOM_PLANE_V], src->stride[AOM_PLANE_V],
            dst->planes[AOM_PLANE_V], dst->stride[AOM_PLANE_V], dst->d_w / 2,
            dst->d_h / 2, src->bit_depth);

  luma = dst->planes[AOM_PLANE_Y];
  cb = dst->planes[AOM_PLANE_U];
  cr = dst->planes[AOM_PLANE_V];
  luma_stride = (dst->fmt == AOM_IMG_FMT_I42016) ? dst->stride[AOM_PLANE_Y] / 2
                                                 : dst->stride[AOM_PLANE_Y];
  chroma_stride = (dst->fmt == AOM_IMG_FMT_I42016)
                      ? dst->stride[AOM_PLANE_U] / 2
                      : dst->stride[AOM_PLANE_U];
  width = dst->d_w;
  height = dst->d_h;
  params->bit_depth = dst->bit_depth;

  int use_high_bit_depth = 0;

  if (dst->fmt == AOM_IMG_FMT_I42016) {
    use_high_bit_depth = 1;
  }

  av1_add_film_grain_run(params, luma, cb, cr, height, width, luma_stride,
                         chroma_stride, use_high_bit_depth);
  return;
}

void av1_add_film_grain_run(aom_film_grain_t *params, uint8_t *luma,
                            uint8_t *cb, uint8_t *cr, int height, int width,
                            int luma_stride, int chroma_stride,
                            int use_high_bit_depth) {
  int **pred_pos_luma;
  int **pred_pos_chroma;
  int *luma_grain_block;
  int *cb_grain_block;
  int *cr_grain_block;

  int *y_line_buf;
  int *cb_line_buf;
  int *cr_line_buf;

  int *y_col_buf;
  int *cb_col_buf;
  int *cr_col_buf;

  random_register = params->random_seed;

  int left_pad = 3;
  int right_pad = 3;  // padding to offset for AR coefficients
  int top_pad = 3;
  int bottom_pad = 0;

  int ar_padding = 3;  // maximum lag used for stabilization of AR coefficients

  // Initial padding is only needed for generation of
  // film grain templates (to stabilize the AR process)
  // Only a 64x64 luma and 32x32 chroma part of a template
  // is used later for adding grain, padding can be discarded

  int luma_block_size_y =
      top_pad + 2 * ar_padding + luma_subblock_size * 2 + bottom_pad;
  int luma_block_size_x = left_pad + 2 * ar_padding + luma_subblock_size * 2 +
                          2 * ar_padding + right_pad;

  int chroma_block_size_y =
      top_pad + ar_padding + chroma_subblock_size * 2 + bottom_pad;
  int chroma_block_size_x =
      left_pad + ar_padding + chroma_subblock_size * 2 + ar_padding + right_pad;

  int luma_grain_stride = luma_block_size_x;
  int chroma_grain_stride = chroma_block_size_x;

  int overlap = params->overlap_flag;
  int bit_depth = params->bit_depth;

  grain_center = 128 << (bit_depth - 8);
  grain_min = 0 - grain_center;
  grain_max = (256 << (bit_depth - 8)) - 1 - grain_center;

  init_arrays(params, luma_stride, chroma_stride, &pred_pos_luma,
              &pred_pos_chroma, &luma_grain_block, &cb_grain_block,
              &cr_grain_block, &y_line_buf, &cb_line_buf, &cr_line_buf,
              &y_col_buf, &cb_col_buf, &cr_col_buf,
              luma_block_size_y * luma_block_size_x,
              chroma_block_size_y * chroma_block_size_x);

  generate_luma_grain_block(params, pred_pos_luma, luma_grain_block,
                            luma_block_size_y, luma_block_size_x,
                            luma_grain_stride, left_pad, top_pad, right_pad,
                            bottom_pad);

  generate_chroma_grain_blocks(
      params,
      //                               pred_pos_luma,
      pred_pos_chroma, luma_grain_block, cb_grain_block, cr_grain_block,
      luma_grain_stride, chroma_block_size_y, chroma_block_size_x,
      chroma_grain_stride, left_pad, top_pad, right_pad, bottom_pad);

  init_scaling_function(params->scaling_points_y, params->num_y_points,
                        scaling_lut_y);

  if (params->chroma_scaling_from_luma) {
    memcpy(scaling_lut_cb, scaling_lut_y, sizeof(*scaling_lut_y) * 256);
    memcpy(scaling_lut_cr, scaling_lut_y, sizeof(*scaling_lut_y) * 256);
  } else {
    init_scaling_function(params->scaling_points_cb, params->num_cb_points,
                          scaling_lut_cb);
    init_scaling_function(params->scaling_points_cr, params->num_cr_points,
                          scaling_lut_cr);
  }
  for (int y = 0; y < height / 2; y += chroma_subblock_size) {
    init_random_generator(y * 2, params->random_seed);

    for (int x = 0; x < width / 2; x += chroma_subblock_size) {
      int offset_y = get_random_number(8);
      int offset_x = (offset_y >> 4) & 15;
      offset_y &= 15;

      int luma_offset_y = left_pad + 2 * ar_padding + (offset_y << 1);
      int luma_offset_x = top_pad + 2 * ar_padding + (offset_x << 1);

      int chroma_offset_y = top_pad + ar_padding + offset_y;
      int chroma_offset_x = left_pad + ar_padding + offset_x;

      if (overlap && x) {
        for (int i = 0; i < AOMMIN(chroma_subblock_size + 1, height / 2 - y);
             i++) {
          y_col_buf[(i << 1) * 2] =
              clamp((27 * y_col_buf[(i << 1) * 2] +
                     17 * luma_grain_block[(luma_offset_y + (i << 1)) *
                                               luma_grain_stride +
                                           luma_offset_x] +
                     16) >>
                        5,
                    grain_min, grain_max);

          y_col_buf[(i << 1) * 2 + 1] =
              clamp((17 * y_col_buf[(i << 1) * 2 + 1] +
                     27 * luma_grain_block[(luma_offset_y + (i << 1)) *
                                               luma_grain_stride +
                                           luma_offset_x + 1] +
                     16) >>
                        5,
                    grain_min, grain_max);

          y_col_buf[(i << 1) * 2 + 2] =
              clamp((27 * y_col_buf[(i << 1) * 2 + 2] +
                     17 * luma_grain_block[(luma_offset_y + (i << 1) + 1) *
                                               luma_grain_stride +
                                           luma_offset_x] +
                     16) >>
                        5,
                    grain_min, grain_max);
          y_col_buf[(i << 1) * 2 + 3] =
              clamp((17 * y_col_buf[(i << 1) * 2 + 3] +
                     27 * luma_grain_block[(luma_offset_y + (i << 1) + 1) *
                                               luma_grain_stride +
                                           luma_offset_x + 1] +
                     16) >>
                        5,
                    grain_min, grain_max);

          cb_col_buf[i] = clamp(
              (23 * cb_col_buf[i] +
               22 * cb_grain_block[(chroma_offset_y + i) * chroma_grain_stride +
                                   chroma_offset_x] +
               16) >>
                  5,
              grain_min, grain_max);
          cr_col_buf[i] = clamp(
              (23 * cr_col_buf[i] +
               22 * cr_grain_block[(chroma_offset_y + i) * chroma_grain_stride +
                                   chroma_offset_x] +
               16) >>
                  5,
              grain_min, grain_max);
        }

        int i = y ? 1 : 0;

        if (use_high_bit_depth) {
          add_noise_to_block_hbd(
              params,
              (uint16_t *)luma + ((y + i) << 1) * luma_stride + (x << 1),
              (uint16_t *)cb + (y + i) * chroma_stride + x,
              (uint16_t *)cr + (y + i) * chroma_stride + x, luma_stride,
              chroma_stride, y_col_buf + i * 4, cb_col_buf + i, cr_col_buf + i,
              2, 1, AOMMIN(chroma_subblock_size, height / 2 - y) - i, 1,
              bit_depth);
        } else {
          add_noise_to_block(
              params, luma + ((y + i) << 1) * luma_stride + (x << 1),
              cb + (y + i) * chroma_stride + x,
              cr + (y + i) * chroma_stride + x, luma_stride, chroma_stride,
              y_col_buf + i * 4, cb_col_buf + i, cr_col_buf + i, 2, 1,
              AOMMIN(chroma_subblock_size, height / 2 - y) - i, 1, bit_depth);
        }
      }

      if (overlap && y) {
        if (x) {
          y_line_buf[x << 1] =
              clamp((17 * y_col_buf[0] + 27 * y_line_buf[x << 1] + 16) >> 5,
                    grain_min, grain_max);
          y_line_buf[(x << 1) + 1] = clamp(
              (17 * y_col_buf[1] + 27 * y_line_buf[(x << 1) + 1] + 16) >> 5,
              grain_min, grain_max);
          y_line_buf[luma_stride + (x << 1)] =
              clamp((27 * y_col_buf[2] +
                     17 * y_line_buf[luma_stride + (x << 1)] + 16) >>
                        5,
                    grain_min, grain_max);
          y_line_buf[luma_stride + (x << 1) + 1] =
              clamp((27 * y_col_buf[3] +
                     17 * y_line_buf[luma_stride + (x << 1) + 1] + 16) >>
                        5,
                    grain_min, grain_max);

          cb_line_buf[x] =
              clamp((22 * cb_col_buf[0] + 23 * cb_line_buf[x] + 16) >> 5,
                    grain_min, grain_max);
          cr_line_buf[x] =
              clamp((22 * cr_col_buf[0] + 23 * cr_line_buf[x] + 16) >> 5,
                    grain_min, grain_max);
        }

        for (int j = x ? 1 : 0; j < AOMMIN(chroma_subblock_size, width / 2 - x);
             j++) {
          y_line_buf[(x + j) << 1] =
              clamp((27 * y_line_buf[(x + j) << 1] +
                     17 * luma_grain_block[luma_offset_y * luma_grain_stride +
                                           luma_offset_x + (j << 1)] +
                     16) >>
                        5,
                    grain_min, grain_max);
          y_line_buf[((x + j) << 1) + 1] =
              clamp((27 * y_line_buf[((x + j) << 1) + 1] +
                     17 * luma_grain_block[luma_offset_y * luma_grain_stride +
                                           luma_offset_x + (j << 1) + 1] +
                     16) >>
                        5,
                    grain_min, grain_max);

          y_line_buf[luma_stride + ((x + j) << 1)] = clamp(
              (17 * y_line_buf[luma_stride + ((x + j) << 1)] +
               27 * luma_grain_block[(luma_offset_y + 1) * luma_grain_stride +
                                     luma_offset_x + (j << 1)] +
               16) >>
                  5,
              grain_min, grain_max);
          y_line_buf[luma_stride + ((x + j) << 1) + 1] = clamp(
              (17 * y_line_buf[luma_stride + ((x + j) << 1) + 1] +
               27 * luma_grain_block[(luma_offset_y + 1) * luma_grain_stride +
                                     luma_offset_x + (j << 1) + 1] +
               16) >>
                  5,
              grain_min, grain_max);

          cb_line_buf[x + j] =
              clamp((23 * cb_line_buf[x + j] +
                     22 * cb_grain_block[chroma_offset_y * chroma_grain_stride +
                                         chroma_offset_x + j] +
                     16) >>
                        5,
                    grain_min, grain_max);
          cr_line_buf[x + j] =
              clamp((23 * cr_line_buf[x + j] +
                     22 * cr_grain_block[chroma_offset_y * chroma_grain_stride +
                                         chroma_offset_x + j] +
                     16) >>
                        5,
                    grain_min, grain_max);
        }

        if (use_high_bit_depth) {
          add_noise_to_block_hbd(
              params, (uint16_t *)luma + (y << 1) * luma_stride + (x << 1),
              (uint16_t *)cb + y * chroma_stride + x,
              (uint16_t *)cr + y * chroma_stride + x, luma_stride,
              chroma_stride, y_line_buf + (x << 1), cb_line_buf + x,
              cr_line_buf + x, luma_stride, chroma_stride, 1,
              AOMMIN(chroma_subblock_size, width / 2 - x), bit_depth);
        } else {
          add_noise_to_block(
              params, luma + (y << 1) * luma_stride + (x << 1),
              cb + y * chroma_stride + x, cr + y * chroma_stride + x,
              luma_stride, chroma_stride, y_line_buf + (x << 1),
              cb_line_buf + x, cr_line_buf + x, luma_stride, chroma_stride, 1,
              AOMMIN(chroma_subblock_size, width / 2 - x), bit_depth);
        }
      }

      int i = overlap && y ? 1 : 0;
      int j = overlap && x ? 1 : 0;

      if (use_high_bit_depth) {
        add_noise_to_block_hbd(
            params,
            (uint16_t *)luma + ((y + i) << 1) * luma_stride + ((x + j) << 1),
            (uint16_t *)cb + (y + i) * chroma_stride + x + j,
            (uint16_t *)cr + (y + i) * chroma_stride + x + j, luma_stride,
            chroma_stride,
            luma_grain_block + (luma_offset_y + (i << 1)) * luma_grain_stride +
                luma_offset_x + (j << 1),
            cb_grain_block + (chroma_offset_y + i) * chroma_grain_stride +
                chroma_offset_x + j,
            cr_grain_block + (chroma_offset_y + i) * chroma_grain_stride +
                chroma_offset_x + j,
            luma_grain_stride, chroma_grain_stride,
            AOMMIN(chroma_subblock_size, height / 2 - y) - i,
            AOMMIN(chroma_subblock_size, width / 2 - x) - j, bit_depth);
      } else {
        add_noise_to_block(
            params, luma + ((y + i) << 1) * luma_stride + ((x + j) << 1),
            cb + (y + i) * chroma_stride + x + j,
            cr + (y + i) * chroma_stride + x + j, luma_stride, chroma_stride,
            luma_grain_block + (luma_offset_y + (i << 1)) * luma_grain_stride +
                luma_offset_x + (j << 1),
            cb_grain_block + (chroma_offset_y + i) * chroma_grain_stride +
                chroma_offset_x + j,
            cr_grain_block + (chroma_offset_y + i) * chroma_grain_stride +
                chroma_offset_x + j,
            luma_grain_stride, chroma_grain_stride,
            AOMMIN(chroma_subblock_size, height / 2 - y) - i,
            AOMMIN(chroma_subblock_size, width / 2 - x) - j, bit_depth);
      }

      if (overlap) {
        if (x) {
          y_line_buf[x << 1] = y_col_buf[luma_subblock_size * 2];
          y_line_buf[(x << 1) + 1] = y_col_buf[luma_subblock_size * 2 + 1];
          y_line_buf[luma_stride + (x << 1)] =
              y_col_buf[luma_subblock_size * 2 + 2];
          y_line_buf[luma_stride + (x << 1) + 1] =
              y_col_buf[luma_subblock_size * 2 + 3];

          cb_line_buf[x] = cb_col_buf[chroma_subblock_size];
          cr_line_buf[x] = cr_col_buf[chroma_subblock_size];
        }

        for (int m = x ? 1 : 0; m < AOMMIN(chroma_subblock_size, width / 2 - x);
             m++) {
          y_line_buf[(x + m) << 1] =
              luma_grain_block[(luma_offset_y + luma_subblock_size) *
                                   luma_grain_stride +
                               luma_offset_x + (m << 1)];
          y_line_buf[((x + m) << 1) + 1] =
              luma_grain_block[(luma_offset_y + luma_subblock_size) *
                                   luma_grain_stride +
                               luma_offset_x + (m << 1) + 1];
          y_line_buf[luma_stride + ((x + m) << 1)] =
              luma_grain_block[(luma_offset_y + luma_subblock_size + 1) *
                                   luma_grain_stride +
                               luma_offset_x + (m << 1)];
          y_line_buf[luma_stride + ((x + m) << 1) + 1] =
              luma_grain_block[(luma_offset_y + luma_subblock_size + 1) *
                                   luma_grain_stride +
                               luma_offset_x + (m << 1) + 1];

          cb_line_buf[x + m] =
              cb_grain_block[(chroma_offset_y + chroma_subblock_size) *
                                 chroma_grain_stride +
                             chroma_offset_x + m];
          cr_line_buf[x + m] =
              cr_grain_block[(chroma_offset_y + chroma_subblock_size) *
                                 chroma_grain_stride +
                             chroma_offset_x + m];
        }

        for (int n = 0; n < AOMMIN(chroma_subblock_size + 1, height / 2 - y);
             n++) {
          y_col_buf[(n << 1) * 2] =
              luma_grain_block[(luma_offset_y + (n << 1)) * luma_grain_stride +
                               luma_offset_x + luma_subblock_size];
          y_col_buf[(n << 1) * 2 + 1] =
              luma_grain_block[(luma_offset_y + (n << 1)) * luma_grain_stride +
                               luma_offset_x + luma_subblock_size + 1];

          y_col_buf[((n << 1) + 1) * 2] =
              luma_grain_block[(luma_offset_y + (n << 1) + 1) *
                                   luma_grain_stride +
                               luma_offset_x + luma_subblock_size];
          y_col_buf[((n << 1) + 1) * 2 + 1] =
              luma_grain_block[(luma_offset_y + (n << 1) + 1) *
                                   luma_grain_stride +
                               luma_offset_x + luma_subblock_size + 1];

          cb_col_buf[n] =
              cb_grain_block[(chroma_offset_y + n) * chroma_grain_stride +
                             chroma_offset_x + chroma_subblock_size];
          cr_col_buf[n] =
              cr_grain_block[(chroma_offset_y + n) * chroma_grain_stride +
                             chroma_offset_x + chroma_subblock_size];
        }
      }
    }
  }

  dealloc_arrays(params, &pred_pos_luma, &pred_pos_chroma, &luma_grain_block,
                 &cb_grain_block, &cr_grain_block, &y_line_buf, &cb_line_buf,
                 &cr_line_buf, &y_col_buf, &cb_col_buf, &cr_col_buf);
}
