##
## Copyright (c) 2017, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
sub av1_common_forward_decls() {
print <<EOF
/*
 * AV1
 */

#include "aom/aom_integer.h"
#include "aom_dsp/txfm_common.h"
#include "av1/common/common.h"
#include "av1/common/enums.h"
#include "av1/common/quant_common.h"
#include "av1/common/filter.h"
#include "av1/common/convolve.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/odintrin.h"

#if CONFIG_LOOP_RESTORATION
#include "av1/common/restoration.h"
#endif

#if CONFIG_CFL
#include "av1/common/cfl.h"
#endif

struct macroblockd;

/* Encoder forward decls */
struct macroblock;
struct txfm_param;
struct aom_variance_vtable;
struct search_site_config;
struct mv;
union int_mv;
struct yv12_buffer_config;
EOF
}
forward_decls qw/av1_common_forward_decls/;

# functions that are 64 bit only.
$mmx_x86_64 = $sse2_x86_64 = $ssse3_x86_64 = $avx_x86_64 = $avx2_x86_64 = '';
if ($opts{arch} eq "x86_64") {
  $mmx_x86_64 = 'mmx';
  $sse2_x86_64 = 'sse2';
  $ssse3_x86_64 = 'ssse3';
  $avx_x86_64 = 'avx';
  $avx2_x86_64 = 'avx2';
}

#
# 10/12-tap convolution filters
#
add_proto qw/void av1_convolve_horiz/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, ConvolveParams *conv_params";
specialize qw/av1_convolve_horiz ssse3/;

add_proto qw/void av1_convolve_vert/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, ConvolveParams *conv_params";
specialize qw/av1_convolve_vert ssse3/;

if (aom_config("CONFIG_HORZONLY_FRAME_SUPERRES") eq "yes") {
  add_proto qw/void av1_convolve_horiz_rs/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const int16_t *x_filters, const int x0_qn, const int x_step_qn";
  specialize qw/av1_convolve_horiz_rs sse4_1/;

  add_proto qw/void av1_highbd_convolve_horiz_rs/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const int16_t *x_filters, const int x0_qn, const int x_step_qn, int bd";
  specialize qw/av1_highbd_convolve_horiz_rs sse4_1/;
}

add_proto qw/void av1_highbd_convolve_horiz/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, int avg, int bd";
specialize qw/av1_highbd_convolve_horiz sse4_1/;
add_proto qw/void av1_highbd_convolve_vert/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, int avg, int bd";
specialize qw/av1_highbd_convolve_vert sse4_1/;

#
# txb
#
if (aom_config("CONFIG_LV_MAP") eq "yes") {
  add_proto qw/void av1_get_br_level_counts/, "const uint8_t *const levels, const int width, const int height, uint8_t *const level_counts";
  specialize qw/av1_get_br_level_counts sse2/;
}

#
# Inverse dct
#
add_proto qw/void av1_iht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";
specialize qw/av1_iht4x4_16_add sse2/;

add_proto qw/void av1_iht4x8_32_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";
specialize qw/av1_iht4x8_32_add sse2/;

add_proto qw/void av1_iht8x4_32_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";
specialize qw/av1_iht8x4_32_add sse2/;

add_proto qw/void av1_iht8x16_128_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";
specialize qw/av1_iht8x16_128_add sse2/;

add_proto qw/void av1_iht16x8_128_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";
specialize qw/av1_iht16x8_128_add sse2/;

add_proto qw/void av1_iht16x32_512_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";
specialize qw/av1_iht16x32_512_add sse2/;

add_proto qw/void av1_iht32x16_512_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";
specialize qw/av1_iht32x16_512_add sse2/;

add_proto qw/void av1_iht4x16_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_iht16x4_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_iht8x32_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_iht32x8_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_iht8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";
specialize qw/av1_iht8x8_64_add sse2/;

add_proto qw/void av1_iht16x16_256_add/, "const tran_low_t *input, uint8_t *output, int pitch, const struct txfm_param *param";
specialize qw/av1_iht16x16_256_add sse2 avx2/;

add_proto qw/void av1_iht32x32_1024_add/, "const tran_low_t *input, uint8_t *output, int pitch, const struct txfm_param *param";

add_proto qw/void av1_iht32x32_1024_add/, "const tran_low_t *input, uint8_t *output, int pitch, const struct txfm_param *param";

if (aom_config("CONFIG_TX64X64") eq "yes") {
  add_proto qw/void av1_iht64x64_4096_add/, "const tran_low_t *input, uint8_t *output, int pitch, const struct txfm_param *param";
  add_proto qw/void av1_iht32x64_2048_add/, "const tran_low_t *input, uint8_t *output, int pitch, const struct txfm_param *param";
  add_proto qw/void av1_iht64x32_2048_add/, "const tran_low_t *input, uint8_t *output, int pitch, const struct txfm_param *param";
  add_proto qw/void av1_iht16x64_1024_add/, "const tran_low_t *input, uint8_t *output, int pitch, const struct txfm_param *param";
  add_proto qw/void av1_iht64x16_1024_add/, "const tran_low_t *input, uint8_t *output, int pitch, const struct txfm_param *param";
}

if (aom_config("CONFIG_NEW_QUANT") eq "yes") {
  add_proto qw/void quantize_nuq/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, const int16_t *dequant_ptr, int dq, const cuml_bins_type_nuq *cuml_bins_ptr, const dequant_val_type_nuq *dequant_val, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, uint16_t *eob_ptr, const int16_t *scan, const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr";

  add_proto qw/void quantize_fp_nuq/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *quant_ptr, const int16_t *dequant_ptr, int dq, const cuml_bins_type_nuq *cuml_bins_ptr, const dequant_val_type_nuq *dequant_val, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, uint16_t *eob_ptr, const int16_t *scan, const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr";

  add_proto qw/void quantize_32x32_nuq/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, const int16_t *dequant_ptr, int dq, const cuml_bins_type_nuq *cuml_bins_ptr, const dequant_val_type_nuq *dequant_val, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, uint16_t *eob_ptr, const int16_t *scan, const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr";

  add_proto qw/void quantize_32x32_fp_nuq/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *quant_ptr, const int16_t *dequant_ptr, int dq, const cuml_bins_type_nuq *cuml_bins_ptr, const dequant_val_type_nuq *dequant_val, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, uint16_t *eob_ptr, const int16_t *scan, const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr";

  if (aom_config("CONFIG_TX64X64") eq "yes") {
    add_proto qw/void quantize_64x64_nuq/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, const int16_t *dequant_ptr, int dq, const cuml_bins_type_nuq *cuml_bins_ptr, const dequant_val_type_nuq *dequant_val, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, uint16_t *eob_ptr, const int16_t *scan, const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr";

    add_proto qw/void quantize_64x64_fp_nuq/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *quant_ptr, const int16_t *dequant_ptr, int dq, const cuml_bins_type_nuq *cuml_bins_ptr, const dequant_val_type_nuq *dequant_val, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, uint16_t *eob_ptr, const int16_t *scan, const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr";
  }
}

# FILTER_INTRA predictor functions
if (aom_config("CONFIG_FILTER_INTRA") eq "yes") {
  add_proto qw/void av1_dc_filter_predictor/, "uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size, const uint8_t *above, const uint8_t *left";
  add_proto qw/void av1_v_filter_predictor/, "uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size, const uint8_t *above, const uint8_t *left";
  add_proto qw/void av1_h_filter_predictor/, "uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size, const uint8_t *above, const uint8_t *left";
  add_proto qw/void av1_d117_filter_predictor/, "uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size, const uint8_t *above, const uint8_t *left";
  add_proto qw/void av1_d153_filter_predictor/, "uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size, const uint8_t *above, const uint8_t *left";
  add_proto qw/void av1_paeth_filter_predictor/, "uint8_t *dst, ptrdiff_t stride, TX_SIZE tx_size, const uint8_t *above, const uint8_t *left";

  # High bitdepth functions
  add_proto qw/void av1_highbd_dc_filter_predictor/, "uint16_t *dst, ptrdiff_t stride, TX_SIZE tx_size, const uint16_t *above, const uint16_t *left, int bd";
  add_proto qw/void av1_highbd_v_filter_predictor/, "uint16_t *dst, ptrdiff_t stride, TX_SIZE tx_size, const uint16_t *above, const uint16_t *left, int bd";
  add_proto qw/void av1_highbd_h_filter_predictor/, "uint16_t *dst, ptrdiff_t stride, TX_SIZE tx_size, const uint16_t *above, const uint16_t *left, int bd";
  add_proto qw/void av1_highbd_d117_filter_predictor/, "uint16_t *dst, ptrdiff_t stride, TX_SIZE tx_size, const uint16_t *above, const uint16_t *left, int bd";
  add_proto qw/void av1_highbd_d153_filter_predictor/, "uint16_t *dst, ptrdiff_t stride, TX_SIZE tx_size, const uint16_t *above, const uint16_t *left, int bd";
  add_proto qw/void av1_highbd_paeth_filter_predictor/, "uint16_t *dst, ptrdiff_t stride, TX_SIZE tx_size, const uint16_t *above, const uint16_t *left, int bd";
}

# High bitdepth functions

#
# Sub Pixel Filters
#
add_proto qw/void av1_highbd_convolve_copy/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";

add_proto qw/void av1_highbd_convolve_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";

add_proto qw/void av1_highbd_convolve8/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
specialize qw/av1_highbd_convolve8/, "$sse2_x86_64";

add_proto qw/void av1_highbd_convolve8_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
specialize qw/av1_highbd_convolve8_horiz/, "$sse2_x86_64";

add_proto qw/void av1_highbd_convolve8_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
specialize qw/av1_highbd_convolve8_vert/, "$sse2_x86_64";

add_proto qw/void av1_highbd_convolve8_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
specialize qw/av1_highbd_convolve8_avg/, "$sse2_x86_64";

add_proto qw/void av1_highbd_convolve8_avg_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
specialize qw/av1_highbd_convolve8_avg_horiz/, "$sse2_x86_64";

add_proto qw/void av1_highbd_convolve8_avg_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
specialize qw/av1_highbd_convolve8_avg_vert/, "$sse2_x86_64";

#
# dct
#
add_proto qw/void av1_highbd_iht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_highbd_iht4x8_32_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_highbd_iht8x4_32_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_highbd_iht8x16_128_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_highbd_iht16x8_128_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_highbd_iht16x32_512_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_highbd_iht32x16_512_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_highbd_iht4x16_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_highbd_iht16x4_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_highbd_iht8x32_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_highbd_iht32x8_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_highbd_iht8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, const struct txfm_param *param";

add_proto qw/void av1_highbd_iht16x16_256_add/, "const tran_low_t *input, uint8_t *output, int pitch, const struct txfm_param *param";

#inv txfm
add_proto qw/void av1_inv_txfm_add/, "const tran_low_t *dqcoeff, uint8_t *dst, int stride, const TxfmParam *txfm_param";
specialize qw/av1_inv_txfm_add sse2/;

add_proto qw/void av1_inv_txfm2d_add_4x8/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_8x4/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_8x16/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_16x8/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_16x32/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_32x16/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_4x4/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
specialize qw/av1_inv_txfm2d_add_4x4 sse4_1/;
add_proto qw/void av1_inv_txfm2d_add_8x8/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
specialize qw/av1_inv_txfm2d_add_8x8 sse4_1/;
add_proto qw/void av1_inv_txfm2d_add_16x16/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
specialize qw/av1_inv_txfm2d_add_16x16 sse4_1/;
add_proto qw/void av1_inv_txfm2d_add_32x32/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
specialize qw/av1_inv_txfm2d_add_32x32 avx2/;

if (aom_config("CONFIG_TX64X64") eq "yes") {
  add_proto qw/void av1_inv_txfm2d_add_64x64/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
  add_proto qw/void av1_inv_txfm2d_add_32x64/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
  add_proto qw/void av1_inv_txfm2d_add_64x32/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
  add_proto qw/void av1_inv_txfm2d_add_16x64/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
  add_proto qw/void av1_inv_txfm2d_add_64x16/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";

  specialize qw/av1_inv_txfm2d_add_64x64 sse4_1/;
}
add_proto qw/void av1_inv_txfm2d_add_4x16/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_16x4/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_8x32/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";
add_proto qw/void av1_inv_txfm2d_add_32x8/, "const int32_t *input, uint16_t *output, int stride, TX_TYPE tx_type, int bd";

#
# Encoder functions below this point.
#
if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {

  # ENCODEMB INVOKE

  # the transform coefficients are held in 32-bit
  # values, so the assembler code for  av1_block_error can no longer be used.
  add_proto qw/int64_t av1_block_error/, "const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz";
  specialize qw/av1_block_error avx2/;

  add_proto qw/void av1_quantize_fp/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/av1_quantize_fp sse2 avx2/;

  add_proto qw/void av1_quantize_fp_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  specialize qw/av1_quantize_fp_32x32 avx2/;

  if (aom_config("CONFIG_TX64X64") eq "yes") {
    add_proto qw/void av1_quantize_fp_64x64/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
  }

  # fdct functions

  add_proto qw/void av1_fht4x4/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
  specialize qw/av1_fht4x4 sse2/;

  add_proto qw/void av1_fwht4x4/, "const int16_t *input, tran_low_t *output, int stride";

  add_proto qw/void av1_fht8x8/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
  specialize qw/av1_fht8x8 sse2/;

  add_proto qw/void av1_fht16x16/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
  specialize qw/av1_fht16x16 sse2 avx2/;

  add_proto qw/void av1_fht32x32/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
  specialize qw/av1_fht32x32 sse2 avx2/;

  if (aom_config("CONFIG_TX64X64") eq "yes") {
    add_proto qw/void av1_fht64x64/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
    add_proto qw/void av1_fht32x64/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
    add_proto qw/void av1_fht64x32/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
    add_proto qw/void av1_fht16x64/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
    add_proto qw/void av1_fht64x16/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
  }

  add_proto qw/void av1_fht4x8/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
  specialize qw/av1_fht4x8 sse2/;

  add_proto qw/void av1_fht8x4/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
  specialize qw/av1_fht8x4 sse2/;

  add_proto qw/void av1_fht8x16/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
  specialize qw/av1_fht8x16 sse2/;

  add_proto qw/void av1_fht16x8/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
  specialize qw/av1_fht16x8 sse2/;

  add_proto qw/void av1_fht16x32/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
  specialize qw/av1_fht16x32 sse2/;

  add_proto qw/void av1_fht32x16/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";
  specialize qw/av1_fht32x16 sse2/;

  add_proto qw/void av1_fht4x16/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";

  add_proto qw/void av1_fht16x4/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";

  add_proto qw/void av1_fht8x32/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";

  add_proto qw/void av1_fht32x8/, "const int16_t *input, tran_low_t *output, int stride, struct txfm_param *param";


  add_proto qw/void av1_fwd_idtx/, "const int16_t *src_diff, tran_low_t *coeff, int stride, int bsx, int bsy, TX_TYPE tx_type";

  #fwd txfm
  add_proto qw/void av1_lowbd_fwd_txfm/, "const int16_t *src_diff, tran_low_t *coeff, int diff_stride, TxfmParam *txfm_param";
  specialize qw/av1_lowbd_fwd_txfm sse2/;

  add_proto qw/void av1_fwd_txfm2d_4x8/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  add_proto qw/void av1_fwd_txfm2d_8x4/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  add_proto qw/void av1_fwd_txfm2d_8x16/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  add_proto qw/void av1_fwd_txfm2d_16x8/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  add_proto qw/void av1_fwd_txfm2d_16x32/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  add_proto qw/void av1_fwd_txfm2d_32x16/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  add_proto qw/void av1_fwd_txfm2d_4x16/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  add_proto qw/void av1_fwd_txfm2d_16x4/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  add_proto qw/void av1_fwd_txfm2d_8x32/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  add_proto qw/void av1_fwd_txfm2d_32x8/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  add_proto qw/void av1_fwd_txfm2d_4x4/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_4x4 sse4_1/;
  add_proto qw/void av1_fwd_txfm2d_8x8/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_8x8 sse4_1/;
  add_proto qw/void av1_fwd_txfm2d_16x16/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_16x16 sse4_1/;
  add_proto qw/void av1_fwd_txfm2d_32x32/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  specialize qw/av1_fwd_txfm2d_32x32 sse4_1/;

  if (aom_config("CONFIG_TX64X64") eq "yes") {
    add_proto qw/void av1_fwd_txfm2d_64x64/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
    add_proto qw/void av1_fwd_txfm2d_32x64/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
    add_proto qw/void av1_fwd_txfm2d_64x32/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
    add_proto qw/void av1_fwd_txfm2d_16x64/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
    add_proto qw/void av1_fwd_txfm2d_64x16/, "const int16_t *input, int32_t *output, int stride, TX_TYPE tx_type, int bd";
  }
  #
  # Motion search
  #
  add_proto qw/int av1_full_search_sad/, "const struct macroblock *x, const struct mv *ref_mv, int sad_per_bit, int distance, const struct aom_variance_vtable *fn_ptr, const struct mv *center_mv, struct mv *best_mv";
  specialize qw/av1_full_search_sad sse3 sse4_1/;
  $av1_full_search_sad_sse3=av1_full_search_sadx3;
  $av1_full_search_sad_sse4_1=av1_full_search_sadx8;

  add_proto qw/int av1_diamond_search_sad/, "struct macroblock *x, const struct search_site_config *cfg,  struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct aom_variance_vtable *fn_ptr, const struct mv *center_mv";

  add_proto qw/int av1_full_range_search/, "const struct macroblock *x, const struct search_site_config *cfg, struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct aom_variance_vtable *fn_ptr, const struct mv *center_mv";

  add_proto qw/void av1_temporal_filter_apply/, "uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count";
  specialize qw/av1_temporal_filter_apply sse2 msa/;

  if (aom_config("CONFIG_AOM_QM") eq "yes") {
    add_proto qw/void av1_quantize_b/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t * iqm_ptr, int log_scale";
  } else {
    add_proto qw/void av1_quantize_b/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, int log_scale";
  }

  # ENCODEMB INVOKE
  if (aom_config("CONFIG_NEW_QUANT") eq "yes") {
    add_proto qw/void highbd_quantize_nuq/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, const int16_t *dequant_ptr, int dq, const cuml_bins_type_nuq *cuml_bins_ptr, const dequant_val_type_nuq *dequant_val, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, uint16_t *eob_ptr, const int16_t *scan, const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr";

    add_proto qw/void highbd_quantize_fp_nuq/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *quant_ptr, const int16_t *dequant_ptr, int dq, const cuml_bins_type_nuq *cuml_bins_ptr, const dequant_val_type_nuq *dequant_val, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, uint16_t *eob_ptr, const int16_t *scan, const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr";

    add_proto qw/void highbd_quantize_32x32_nuq/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, const int16_t *dequant_ptr, int dq, const cuml_bins_type_nuq *cuml_bins_ptr, const dequant_val_type_nuq *dequant_val, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, uint16_t *eob_ptr, const int16_t *scan, const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr";

    add_proto qw/void highbd_quantize_32x32_fp_nuq/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *quant_ptr, const int16_t *dequant_ptr, int dq, const cuml_bins_type_nuq *cuml_bins_ptr, const dequant_val_type_nuq *dequant_val, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, uint16_t *eob_ptr, const int16_t *scan, const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr";

    if (aom_config("CONFIG_TX64X64") eq "yes") {
      add_proto qw/void highbd_quantize_64x64_nuq/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, const int16_t *dequant_ptr, int dq, const cuml_bins_type_nuq *cuml_bins_ptr, const dequant_val_type_nuq *dequant_val, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, uint16_t *eob_ptr, const int16_t *scan, const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr";

      add_proto qw/void highbd_quantize_64x64_fp_nuq/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *quant_ptr, const int16_t *dequant_ptr, int dq, const cuml_bins_type_nuq *cuml_bins_ptr, const dequant_val_type_nuq *dequant_val, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, uint16_t *eob_ptr, const int16_t *scan, const qm_val_t *qm_ptr, const qm_val_t *iqm_ptr";
    }
  }

  add_proto qw/int64_t av1_highbd_block_error/, "const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz, int bd";
  specialize qw/av1_highbd_block_error sse2/;

  add_proto qw/void av1_highbd_temporal_filter_apply/, "uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count";

  add_proto qw/void av1_highbd_quantize_fp/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, int log_scale";
  specialize qw/av1_highbd_quantize_fp sse4_1 avx2/;

  add_proto qw/void av1_highbd_fwht4x4/, "const int16_t *input, tran_low_t *output, int stride";

  # End av1_high encoder functions

  # txb
  if (aom_config("CONFIG_LV_MAP") eq "yes") {
    add_proto qw/void av1_get_nz_map_contexts/, "const uint8_t *const levels, const int16_t *const scan, const uint16_t eob, const TX_SIZE tx_size, const TX_TYPE tx_type, int8_t *const coeff_contexts";
    specialize qw/av1_get_nz_map_contexts sse2/;
  }

  add_proto qw/uint64_t av1_wedge_sse_from_residuals/, "const int16_t *r1, const int16_t *d, const uint8_t *m, int N";
  specialize qw/av1_wedge_sse_from_residuals sse2/;
  add_proto qw/int av1_wedge_sign_from_residuals/, "const int16_t *ds, const uint8_t *m, int N, int64_t limit";
  specialize qw/av1_wedge_sign_from_residuals sse2/;
  add_proto qw/void av1_wedge_compute_delta_squares/, "int16_t *d, const int16_t *a, const int16_t *b, int N";
  specialize qw/av1_wedge_compute_delta_squares sse2/;

}
# end encoder functions

# Deringing Functions

add_proto qw/int cdef_find_dir/, "const uint16_t *img, int stride, int32_t *var, int coeff_shift";
add_proto qw/void cdef_filter_block/, "uint8_t *dst8, uint16_t *dst16, int dstride, const uint16_t *in, int pri_strength, int sec_strength, int dir, int pri_damping, int sec_damping, int bsize, int max, int coeff_shift";

add_proto qw/void copy_rect8_8bit_to_16bit/, "uint16_t *dst, int dstride, const uint8_t *src, int sstride, int v, int h";
add_proto qw/void copy_rect8_16bit_to_16bit/, "uint16_t *dst, int dstride, const uint16_t *src, int sstride, int v, int h";

# VS compiling for 32 bit targets does not support vector types in
# structs as arguments, which makes the v256 type of the intrinsics
# hard to support, so optimizations for this target are disabled.
if ($opts{config} !~ /libs-x86-win32-vs.*/) {
  specialize qw/cdef_find_dir sse2 ssse3 sse4_1 avx2 neon/;
  specialize qw/cdef_filter_block sse2 ssse3 sse4_1 avx2 neon/;
  specialize qw/copy_rect8_8bit_to_16bit sse2 ssse3 sse4_1 avx2 neon/;
  specialize qw/copy_rect8_16bit_to_16bit sse2 ssse3 sse4_1 avx2 neon/;
}

# WARPED_MOTION / GLOBAL_MOTION functions

add_proto qw/void av1_warp_affine/, "const int32_t *mat, const uint8_t *ref, int width, int height, int stride, uint8_t *pred, int p_col, int p_row, int p_width, int p_height, int p_stride, int subsampling_x, int subsampling_y, ConvolveParams *conv_params, int16_t alpha, int16_t beta, int16_t gamma, int16_t delta";

if (aom_config("CONFIG_JNT_COMP") eq "yes") {
  if (aom_config("CONFIG_JNT_COMP") eq "yes") {
    specialize qw/av1_warp_affine sse4_1/;
  }
} else {
  specialize qw/av1_warp_affine sse2 ssse3/;
}

  add_proto qw/void av1_highbd_warp_affine/, "const int32_t *mat, const uint16_t *ref, int width, int height, int stride, uint16_t *pred, int p_col, int p_row, int p_width, int p_height, int p_stride, int subsampling_x, int subsampling_y, int bd, ConvolveParams *conv_params, int16_t alpha, int16_t beta, int16_t gamma, int16_t delta";

if (aom_config("CONFIG_JNT_COMP") eq "yes") {
  if (aom_config("CONFIG_JNT_COMP") eq "yes") {
    specialize qw/av1_highbd_warp_affine sse4_1/;
  }
} else {
  specialize qw/av1_highbd_warp_affine ssse3/;
}


if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {
  add_proto qw/double compute_cross_correlation/, "unsigned char *im1, int stride1, int x1, int y1, unsigned char *im2, int stride2, int x2, int y2";
  specialize qw/compute_cross_correlation sse4_1/;
}

# LOOP_RESTORATION functions

if (aom_config("CONFIG_LOOP_RESTORATION") eq "yes") {
  add_proto qw/void apply_selfguided_restoration/, "const uint8_t *dat, int width, int height, int stride, int eps, const int *xqd, uint8_t *dst, int dst_stride, int32_t *tmpbuf, int bit_depth, int highbd";
  specialize qw/apply_selfguided_restoration sse4_1 avx2/;

  add_proto qw/void av1_selfguided_restoration/, "const uint8_t *dgd, int width, int height, int stride, int32_t *flt1, int32_t *flt2, int flt_stride, const sgr_params_type *params, int bit_depth, int highbd";
  specialize qw/av1_selfguided_restoration sse4_1 avx2/;
}

# CONVOLVE_ROUND/COMPOUND_ROUND functions
add_proto qw/void av1_convolve_2d/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params";
specialize qw/av1_convolve_2d sse2 avx2/;
add_proto qw/void av1_convolve_2d_sr/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params";
specialize qw/av1_convolve_2d_sr c sse2 avx2/;
add_proto qw/void av1_convolve_rounding/, "const int32_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, int bits";
specialize qw/av1_convolve_rounding avx2/;

add_proto qw/void av1_convolve_2d_copy/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params";
specialize qw/av1_convolve_2d_copy sse2/;
add_proto qw/void av1_convolve_2d_copy_sr/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params";
specialize qw/av1_convolve_2d_copy_sr sse2/;
add_proto qw/void av1_convolve_x/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params";
specialize qw/av1_convolve_x sse2 avx2/;
add_proto qw/void av1_convolve_y/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params";
specialize qw/av1_convolve_y sse2 avx2/;
add_proto qw/void av1_convolve_x_sr/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params";
specialize qw/av1_convolve_x_sr c sse2 avx2/;
add_proto qw/void av1_convolve_y_sr/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params";
specialize qw/av1_convolve_y_sr c sse2 avx2/;

add_proto qw/void av1_convolve_2d_scale/, "const uint8_t *src, int src_stride, CONV_BUF_TYPE *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_qn, const int x_step_qn, const int subpel_y_q4, const int y_step_qn, ConvolveParams *conv_params";
specialize qw/av1_convolve_2d_scale sse4_1/;

if (aom_config("CONFIG_JNT_COMP") eq "yes") {
  add_proto qw/void av1_jnt_convolve_2d/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params";
  specialize qw/av1_jnt_convolve_2d sse4_1/;

  add_proto qw/void av1_jnt_convolve_2d_copy/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params";
  specialize qw/av1_jnt_convolve_2d_copy sse2/;

  add_proto qw/void av1_jnt_convolve_x/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params";
  specialize qw/av1_jnt_convolve_x sse4_1/;

  add_proto qw/void av1_jnt_convolve_y/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params";
  specialize qw/av1_jnt_convolve_y sse4_1/;
}

add_proto qw/void av1_highbd_convolve_2d/, "const uint16_t *src, int src_stride, CONV_BUF_TYPE *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params, int bd";
specialize qw/av1_highbd_convolve_2d ssse3 avx2/;
add_proto qw/void av1_highbd_convolve_rounding/, "const int32_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, int bits, int bd";
specialize qw/av1_highbd_convolve_rounding avx2/;

add_proto qw/void av1_highbd_convolve_2d_scale/, "const uint16_t *src, int src_stride, CONV_BUF_TYPE *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int x_step_qn, const int subpel_y_q4, const int y_step_qn, ConvolveParams *conv_params, int bd";
specialize qw/av1_highbd_convolve_2d_scale sse4_1/;

if (aom_config("CONFIG_JNT_COMP") eq "yes") {
  add_proto qw/void av1_highbd_jnt_convolve_2d/, "const uint16_t *src, int src_stride, CONV_BUF_TYPE *dst, int dst_stride, int w, int h, InterpFilterParams *filter_params_x, InterpFilterParams *filter_params_y, const int subpel_x_q4, const int subpel_y_q4, ConvolveParams *conv_params, int bd";
  specialize qw/av1_highbd_jnt_convolve_2d sse4_1/;
}

# INTRA_EDGE functions
if (aom_config("CONFIG_INTRA_EDGE") eq "yes") {
  add_proto qw/void av1_filter_intra_edge/, "uint8_t *p, int sz, int strength";
  specialize qw/av1_filter_intra_edge sse4_1/;
  add_proto qw/void av1_upsample_intra_edge/, "uint8_t *p, int sz";
  specialize qw/av1_upsample_intra_edge sse4_1/;

  add_proto qw/void av1_filter_intra_edge_high/, "uint16_t *p, int sz, int strength";
  specialize qw/av1_filter_intra_edge_high sse4_1/;
  add_proto qw/void av1_upsample_intra_edge_high/, "uint16_t *p, int sz, int bd";
  specialize qw/av1_upsample_intra_edge_high sse4_1/;
}

# CFL
if (aom_config("CONFIG_CFL") eq "yes") {
  add_proto qw/void av1_cfl_subtract/, "int16_t *pred_buf_q3, int width, int height, int16_t avg_q3";
  specialize qw/av1_cfl_subtract sse2 avx2/;

  add_proto qw/cfl_subsample_lbd_fn get_subsample_lbd_fn/, "int sub_x, int sub_y";
  specialize qw/get_subsample_lbd_fn ssse3 avx2/;

  add_proto qw/cfl_predict_lbd_fn get_predict_lbd_fn/, "TX_SIZE tx_size";
  specialize qw/get_predict_lbd_fn ssse3 avx2/;

  add_proto qw/cfl_predict_hbd_fn get_predict_hbd_fn/, "TX_SIZE tx_size";
  specialize qw/get_predict_hbd_fn ssse3 avx2/;
}

1;
