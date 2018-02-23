/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// SSSE3 version is optimal for with == 4, we reuse it in AVX2
void cfl_predict_lbd_4_ssse3(const int16_t *pred_buf_q3, uint8_t *dst,
                             int dst_stride, TX_SIZE tx_size, int alpha_q3);

// SSSE3 version is optimal for with == 8, we reuse it in AVX2
void cfl_predict_lbd_8_ssse3(const int16_t *pred_buf_q3, uint8_t *dst,
                             int dst_stride, TX_SIZE tx_size, int alpha_q3);

// SSSE3 version is optimal for with == 16, we reuse it in AVX2
void cfl_predict_lbd_16_ssse3(const int16_t *pred_buf_q3, uint8_t *dst,
                              int dst_stride, TX_SIZE tx_size, int alpha_q3);

// SSSE3 version is optimal for with == 4, we reuse it in AVX2
void cfl_predict_hbd_4_ssse3(const int16_t *pred_buf_q3, uint16_t *dst,
                             int dst_stride, TX_SIZE tx_size, int alpha_q3,
                             int bd);

// SSSE3 version is optimal for with == 8, we reuse it in AVX2
void cfl_predict_hbd_8_ssse3(const int16_t *pred_buf_q3, uint16_t *dst,
                             int dst_stride, TX_SIZE tx_size, int alpha_q3,
                             int bd);
