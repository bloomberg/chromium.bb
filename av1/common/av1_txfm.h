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

#ifndef AV1_TXFM_H_
#define AV1_TXFM_H_

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "./aom_config.h"
#include "av1/common/enums.h"
#include "av1/common/blockd.h"
#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TXFM_STAGE_NUM 12

static const int cos_bit_min = 10;
static const int cos_bit_max = 16;

static const int NewSqrt2Bits = 12;
// 2^12 * sqrt(2)
static const int32_t NewSqrt2 = 5793;
// 2^12 / sqrt(2)
static const int32_t NewInvSqrt2 = 2896;

// cospi_arr[i][j] = (int)round(cos(M_PI*j/128) * (1<<(cos_bit_min+i)));
static const int32_t cospi_arr_data[7][64] = {
  { 1024, 1024, 1023, 1021, 1019, 1016, 1013, 1009, 1004, 999, 993, 987, 980,
    972,  964,  955,  946,  936,  926,  915,  903,  891,  878, 865, 851, 837,
    822,  807,  792,  775,  759,  742,  724,  706,  688,  669, 650, 630, 610,
    590,  569,  548,  526,  505,  483,  460,  438,  415,  392, 369, 345, 321,
    297,  273,  249,  224,  200,  175,  150,  125,  100,  75,  50,  25 },
  { 2048, 2047, 2046, 2042, 2038, 2033, 2026, 2018, 2009, 1998, 1987,
    1974, 1960, 1945, 1928, 1911, 1892, 1872, 1851, 1829, 1806, 1782,
    1757, 1730, 1703, 1674, 1645, 1615, 1583, 1551, 1517, 1483, 1448,
    1412, 1375, 1338, 1299, 1260, 1220, 1179, 1138, 1096, 1053, 1009,
    965,  921,  876,  830,  784,  737,  690,  642,  595,  546,  498,
    449,  400,  350,  301,  251,  201,  151,  100,  50 },
  { 4096, 4095, 4091, 4085, 4076, 4065, 4052, 4036, 4017, 3996, 3973,
    3948, 3920, 3889, 3857, 3822, 3784, 3745, 3703, 3659, 3612, 3564,
    3513, 3461, 3406, 3349, 3290, 3229, 3166, 3102, 3035, 2967, 2896,
    2824, 2751, 2675, 2598, 2520, 2440, 2359, 2276, 2191, 2106, 2019,
    1931, 1842, 1751, 1660, 1567, 1474, 1380, 1285, 1189, 1092, 995,
    897,  799,  700,  601,  501,  401,  301,  201,  101 },
  { 8192, 8190, 8182, 8170, 8153, 8130, 8103, 8071, 8035, 7993, 7946,
    7895, 7839, 7779, 7713, 7643, 7568, 7489, 7405, 7317, 7225, 7128,
    7027, 6921, 6811, 6698, 6580, 6458, 6333, 6203, 6070, 5933, 5793,
    5649, 5501, 5351, 5197, 5040, 4880, 4717, 4551, 4383, 4212, 4038,
    3862, 3683, 3503, 3320, 3135, 2948, 2760, 2570, 2378, 2185, 1990,
    1795, 1598, 1401, 1202, 1003, 803,  603,  402,  201 },
  { 16384, 16379, 16364, 16340, 16305, 16261, 16207, 16143, 16069, 15986, 15893,
    15791, 15679, 15557, 15426, 15286, 15137, 14978, 14811, 14635, 14449, 14256,
    14053, 13842, 13623, 13395, 13160, 12916, 12665, 12406, 12140, 11866, 11585,
    11297, 11003, 10702, 10394, 10080, 9760,  9434,  9102,  8765,  8423,  8076,
    7723,  7366,  7005,  6639,  6270,  5897,  5520,  5139,  4756,  4370,  3981,
    3590,  3196,  2801,  2404,  2006,  1606,  1205,  804,   402 },
  { 32768, 32758, 32729, 32679, 32610, 32522, 32413, 32286, 32138, 31972, 31786,
    31581, 31357, 31114, 30853, 30572, 30274, 29957, 29622, 29269, 28899, 28511,
    28106, 27684, 27246, 26791, 26320, 25833, 25330, 24812, 24279, 23732, 23170,
    22595, 22006, 21403, 20788, 20160, 19520, 18868, 18205, 17531, 16846, 16151,
    15447, 14733, 14010, 13279, 12540, 11793, 11039, 10279, 9512,  8740,  7962,
    7180,  6393,  5602,  4808,  4011,  3212,  2411,  1608,  804 },
  { 65536, 65516, 65457, 65358, 65220, 65043, 64827, 64571, 64277, 63944, 63572,
    63162, 62714, 62228, 61705, 61145, 60547, 59914, 59244, 58538, 57798, 57022,
    56212, 55368, 54491, 53581, 52639, 51665, 50660, 49624, 48559, 47464, 46341,
    45190, 44011, 42806, 41576, 40320, 39040, 37736, 36410, 35062, 33692, 32303,
    30893, 29466, 28020, 26558, 25080, 23586, 22078, 20557, 19024, 17479, 15924,
    14359, 12785, 11204, 9616,  8022,  6424,  4821,  3216,  1608 }
};

//  sinpi_arr_data[i][j] = (int)round((sqrt(2) * sin(kPi/9) * 2 / 3) * (1 <<
//  (cos_bit_min + i)))
static const int32_t sinpi_arr_data[7][5] = {
  { 0, 330, 621, 836, 951 },        { 0, 660, 1241, 1672, 1902 },
  { 0, 1321, 2482, 3344, 3803 },    { 0, 2642, 4965, 6689, 7606 },
  { 0, 5283, 9929, 13377, 15212 },  { 0, 10566, 19858, 26755, 30425 },
  { 0, 21133, 39717, 53510, 60849 }
};

static INLINE const int32_t *cospi_arr(int n) {
  return cospi_arr_data[n - cos_bit_min];
}

static INLINE const int32_t *sinpi_arr(int n) {
  return sinpi_arr_data[n - cos_bit_min];
}

static INLINE int32_t range_check_value(int32_t value, int8_t bit) {
#if CONFIG_COEFFICIENT_RANGE_CHECKING
  const int64_t max_value = (1LL << (bit - 1)) - 1;
  const int64_t min_value = -(1LL << (bit - 1));
  if (value < min_value || value > max_value) {
    fprintf(stderr, "coeff out of bit range, value: %d bit %d\n", value, bit);
    assert(0);
  }
#else
  (void)bit;
#endif
  return value;
}

static INLINE int32_t round_shift(int32_t value, int bit) {
  assert(bit >= 1);
  return (int32_t)(((int64_t)value + (1ll << (bit - 1))) >> bit);
}

static INLINE int32_t half_btf(int32_t w0, int32_t in0, int32_t w1, int32_t in1,
                               int bit) {
  int32_t result_32 = (int32_t)clamp64((int64_t)w0 * in0 + (int64_t)w1 * in1,
                                       (int64_t)INT32_MIN, (int64_t)INT32_MAX);
  return round_shift(result_32, bit);
}

typedef void (*TxfmFunc)(const int32_t *input, int32_t *output, int8_t cos_bit,
                         const int8_t *stage_range);

typedef enum TXFM_TYPE {
  TXFM_TYPE_DCT4,
  TXFM_TYPE_DCT8,
  TXFM_TYPE_DCT16,
  TXFM_TYPE_DCT32,
  TXFM_TYPE_DCT64,
  TXFM_TYPE_ADST4,
  TXFM_TYPE_ADST8,
  TXFM_TYPE_ADST16,
  TXFM_TYPE_ADST32,
  TXFM_TYPE_IDENTITY4,
  TXFM_TYPE_IDENTITY8,
  TXFM_TYPE_IDENTITY16,
  TXFM_TYPE_IDENTITY32,
  TXFM_TYPE_IDENTITY64,
  TXFM_TYPES,
  TXFM_TYPE_INVALID,
} TXFM_TYPE;

typedef struct TXFM_2D_FLIP_CFG {
  TX_SIZE tx_size;
  int ud_flip;  // flip upside down
  int lr_flip;  // flip left to right
  const int8_t *shift;
  int8_t cos_bit_col;
  int8_t cos_bit_row;
  int8_t stage_range_col[MAX_TXFM_STAGE_NUM];
  int8_t stage_range_row[MAX_TXFM_STAGE_NUM];
  TXFM_TYPE txfm_type_col;
  TXFM_TYPE txfm_type_row;
  int stage_num_col;
  int stage_num_row;
} TXFM_2D_FLIP_CFG;

static INLINE void get_flip_cfg(TX_TYPE tx_type, int *ud_flip, int *lr_flip) {
  switch (tx_type) {
    case DCT_DCT:
    case ADST_DCT:
    case DCT_ADST:
    case ADST_ADST:
      *ud_flip = 0;
      *lr_flip = 0;
      break;
    case IDTX:
    case V_DCT:
    case H_DCT:
    case V_ADST:
    case H_ADST:
      *ud_flip = 0;
      *lr_flip = 0;
      break;
    case FLIPADST_DCT:
    case FLIPADST_ADST:
    case V_FLIPADST:
      *ud_flip = 1;
      *lr_flip = 0;
      break;
    case DCT_FLIPADST:
    case ADST_FLIPADST:
    case H_FLIPADST:
      *ud_flip = 0;
      *lr_flip = 1;
      break;
    case FLIPADST_FLIPADST:
      *ud_flip = 1;
      *lr_flip = 1;
      break;
    default:
      *ud_flip = 0;
      *lr_flip = 0;
      assert(0);
  }
}

static INLINE void set_flip_cfg(TX_TYPE tx_type, TXFM_2D_FLIP_CFG *cfg) {
  get_flip_cfg(tx_type, &cfg->ud_flip, &cfg->lr_flip);
}

static INLINE TX_SIZE av1_rotate_tx_size(TX_SIZE tx_size) {
  switch (tx_size) {
    case TX_4X4: return TX_4X4;
    case TX_8X8: return TX_8X8;
    case TX_16X16: return TX_16X16;
    case TX_32X32: return TX_32X32;
#if CONFIG_TX64X64
    case TX_64X64: return TX_64X64;
    case TX_32X64: return TX_64X32;
    case TX_64X32: return TX_32X64;
#endif  // CONFIG_TX64X64
    case TX_4X8: return TX_8X4;
    case TX_8X4: return TX_4X8;
    case TX_8X16: return TX_16X8;
    case TX_16X8: return TX_8X16;
    case TX_16X32: return TX_32X16;
    case TX_32X16: return TX_16X32;
    case TX_4X16: return TX_16X4;
    case TX_16X4: return TX_4X16;
    case TX_8X32: return TX_32X8;
    case TX_32X8: return TX_8X32;
#if CONFIG_TX64X64
    case TX_16X64: return TX_64X16;
    case TX_64X16: return TX_16X64;
#endif  // CONFIG_TX64X64
    default: assert(0); return TX_INVALID;
  }
}

static INLINE TX_TYPE av1_rotate_tx_type(TX_TYPE tx_type) {
  switch (tx_type) {
    case DCT_DCT: return DCT_DCT;
    case ADST_DCT: return DCT_ADST;
    case DCT_ADST: return ADST_DCT;
    case ADST_ADST: return ADST_ADST;
    case FLIPADST_DCT: return DCT_FLIPADST;
    case DCT_FLIPADST: return FLIPADST_DCT;
    case FLIPADST_FLIPADST: return FLIPADST_FLIPADST;
    case ADST_FLIPADST: return FLIPADST_ADST;
    case FLIPADST_ADST: return ADST_FLIPADST;
    case IDTX: return IDTX;
    case V_DCT: return H_DCT;
    case H_DCT: return V_DCT;
    case V_ADST: return H_ADST;
    case H_ADST: return V_ADST;
    case V_FLIPADST: return H_FLIPADST;
    case H_FLIPADST: return V_FLIPADST;
    default: assert(0); return TX_TYPES;
  }
}

// Utility function that returns the log of the ratio of the col and row
// sizes.
static INLINE int get_rect_tx_log_ratio(int col, int row) {
  if (col == row) return 0;
  if (col > row) {
    if (col == row * 2) return 1;
    if (col == row * 4) return 2;
    assert(0 && "Unsupported transform size");
  } else {
    if (row == col * 2) return -1;
    if (row == col * 4) return -2;
    assert(0 && "Unsupported transform size");
  }
  return 0;  // Invalid
}

void av1_gen_fwd_stage_range(int8_t *stage_range_col, int8_t *stage_range_row,
                             const TXFM_2D_FLIP_CFG *cfg, int bd);

void av1_gen_inv_stage_range(int8_t *stage_range_col, int8_t *stage_range_row,
                             const TXFM_2D_FLIP_CFG *cfg, TX_SIZE tx_size,
                             int bd);

void av1_get_fwd_txfm_cfg(TX_TYPE tx_type, TX_SIZE tx_size,
                          TXFM_2D_FLIP_CFG *cfg);
void av1_get_inv_txfm_cfg(TX_TYPE tx_type, TX_SIZE tx_size,
                          TXFM_2D_FLIP_CFG *cfg);
extern const TXFM_TYPE av1_txfm_type_ls[5][TX_TYPES_1D];
extern const int8_t av1_txfm_stage_num_list[TXFM_TYPES];
static INLINE int get_txw_idx(TX_SIZE tx_size) {
  return tx_size_wide_log2[tx_size] - tx_size_wide_log2[0];
}
static INLINE int get_txh_idx(TX_SIZE tx_size) {
  return tx_size_high_log2[tx_size] - tx_size_high_log2[0];
}
#define MAX_TXWH_IDX 5
#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // AV1_TXFM_H_
