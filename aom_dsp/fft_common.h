/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_DSP_FFT_COMMON_H_
#define AOM_DSP_FFT_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!\brief A function pointer for computing 1d fft and ifft.
 *
 * The function will point to an implementation for a specific transform size,
 * and may perform the transforms using vectorized instructions.
 *
 * For a non-vectorized forward transforms of size n, the input and output
 * buffers will be size n. The output takes advantage of conjugate symmetry and
 * packs the results as: [r_0, r_1, ..., r_{n/2}, i_1, ..., i_{n/2-1}], where
 * (r_{j}, i_{j}) is the complex output for index j.
 *
 * An inverse transform will assume that the complex "input" is packed
 * similarly. Its output will be real.
 *
 * Non-vectorized transforms (e.g., on a single row) would use a stride = 1.
 *
 * Vectorized implementations are parallelized along the columns so that the fft
 * can be performed on multiple columns at a time. In such cases the data block
 * for input and output is typically square (n x n) and the stride will
 * correspond to the spacing between rows. At minimum, the input size must be
 * n x simd_vector_length.
 *
 * \param[in]  input   Input buffer. See above for size restrictions.
 * \param[out] output  Output buffer. See above for size restrictions.
 * \param[in]  stride  The spacing in number of elements between rows
 *                     (or elements)
 */
typedef void (*aom_fft_1d_func_t)(const float *input, float *output,
                                  int stride);

// Declare some of the forward non-vectorized transforms which are used in some
// of the vectorized implementations
void aom_fft1d_4_float(const float *input, float *output, int stride);
void aom_fft1d_8_float(const float *input, float *output, int stride);
void aom_fft1d_16_float(const float *input, float *output, int stride);
void aom_fft1d_32_float(const float *input, float *output, int stride);

/**\!brief Function pointer for transposing a matrix of floats.
 *
 * \param[in]  input  Input buffer (size n x n)
 * \param[out] output Output buffer (size n x n)
 * \param[in]  n      Extent of one dimension of the square matrix.
 */
typedef void (*aom_fft_transpose_func_t)(const float *input, float *output,
                                         int n);

/**\!brief Function pointer for re-arranging intermediate 2d transform results.
 *
 * After re-arrangement, the real and imaginary components will be packed
 * tightly next to each other.
 *
 * \param[in]  input  Input buffer (size n x n)
 * \param[out] output Output buffer (size 2 x n x n)
 * \param[in]  n      Extent of one dimension of the square matrix.
 */
typedef void (*aom_fft_unpack_func_t)(const float *input, float *output, int n);

/*!\brief Performs a 2d fft with the given functions.
 *
 * This generator function allows for multiple different implementations of 2d
 * fft with different vector operations, without having to redefine the main
 * body multiple times.
 *
 * \param[in]  input     Input buffer to run the transform on (size n x n)
 * \param[out] temp      Working buffer for computing the transform (size n x n)
 * \param[out] output    Output buffer (size 2 x n x n)
 * \param[in]  tform     Forward transform function
 * \param[in]  transpose Transpose function (for n x n matrix)
 * \param[in]  unpack    Unpack function used to massage outputs to correct form
 * \param[in]  vec_size  Vector size (the transform is done vec_size units at
 *                       a time)
 */
void aom_fft_2d_gen(const float *input, float *temp, float *output, int n,
                    aom_fft_1d_func_t tform, aom_fft_transpose_func_t transpose,
                    aom_fft_unpack_func_t unpack, int vec_size);

/*!\brief Perform a 2d inverse fft with the given helper functions
 *
 * \param[in]  input      Input buffer to run the transform on (size 2 x n x n)
 * \param[out] temp       Working buffer for computations (size 2 x n x n)
 * \param[out] output     Output buffer (size n x n)
 * \param[in]  fft_single Forward transform function (non vectorized)
 * \param[in]  fft_multi  Forward transform function (vectorized)
 * \param[in]  ifft_multi Inverse transform function (vectorized)
 * \param[in]  transpose  Transpose function (for n x n matrix)
 * \param[in]  vec_size   Vector size (the transform is done vec_size
 *                        units at a time)
 */
void aom_ifft_2d_gen(const float *input, float *temp, float *output, int n,
                     aom_fft_1d_func_t fft_single, aom_fft_1d_func_t fft_multi,
                     aom_fft_1d_func_t ifft_multi,
                     aom_fft_transpose_func_t transpose, int vec_size);
#ifdef __cplusplus
}
#endif

// The macros below define 1D fft/ifft for different data types and for
// different simd vector intrinsic types.

#define GEN_FFT_2(ret, suffix, T, T_VEC, load, store)               \
  ret aom_fft1d_2_##suffix(const T *input, T *output, int stride) { \
    const T_VEC i0 = load(input + 0 * stride);                      \
    const T_VEC i1 = load(input + 1 * stride);                      \
    store(output + 0 * stride, i0 + i1);                            \
    store(output + 1 * stride, i0 - i1);                            \
  }

#define GEN_FFT_4(ret, suffix, T, T_VEC, load, store)               \
  ret aom_fft1d_4_##suffix(const T *input, T *output, int stride) { \
    const T_VEC i0 = load(input + 0 * stride);                      \
    const T_VEC i1 = load(input + 1 * stride);                      \
    const T_VEC i2 = load(input + 2 * stride);                      \
    const T_VEC i3 = load(input + 3 * stride);                      \
    const T_VEC w0 = i0 + i2;                                       \
    const T_VEC w1 = i0 - i2;                                       \
    const T_VEC w2 = i1 + i3;                                       \
    const T_VEC w3 = i1 - i3;                                       \
    store(output + 0 * stride, w0 + w2);                            \
    store(output + 1 * stride, w1);                                 \
    store(output + 2 * stride, w0 - w2);                            \
    store(output + 3 * stride, -w3);                                \
  }

#define GEN_FFT_8(ret, suffix, T, T_VEC, load, store, constant)            \
  ret aom_fft1d_8_##suffix(const T *input, T *output, int stride) {        \
    const T_VEC kWeight2 = constant(0.707107f);                            \
    const T_VEC i0 = load(input + 0 * stride);                             \
    const T_VEC i1 = load(input + 1 * stride);                             \
    const T_VEC i2 = load(input + 2 * stride);                             \
    const T_VEC i3 = load(input + 3 * stride);                             \
    const T_VEC i4 = load(input + 4 * stride);                             \
    const T_VEC i5 = load(input + 5 * stride);                             \
    const T_VEC i6 = load(input + 6 * stride);                             \
    const T_VEC i7 = load(input + 7 * stride);                             \
    const T_VEC w0 = i0 + i4;                                              \
    const T_VEC w1 = i0 - i4;                                              \
    const T_VEC w2 = i2 + i6;                                              \
    const T_VEC w3 = i2 - i6;                                              \
    const T_VEC w4 = w0 + w2;                                              \
    const T_VEC w5 = w0 - w2;                                              \
    /* w6 = -w3 is not worth a temporary; it is replaced with -w3 below */ \
    const T_VEC w7 = i1 + i5;                                              \
    const T_VEC w8 = i1 - i5;                                              \
    const T_VEC w9 = i3 + i7;                                              \
    const T_VEC w10 = i3 - i7;                                             \
    const T_VEC w11 = w7 + w9;                                             \
    const T_VEC w12 = w7 - w9;                                             \
    store(output + 0 * stride, w4 + w11);                                  \
    store(output + 1 * stride, w1 + (kWeight2 * w8 - kWeight2 * w10));     \
    store(output + 2 * stride, w5);                                        \
    store(output + 3 * stride, w1 + (-kWeight2 * w8 + kWeight2 * w10));    \
    store(output + 4 * stride, w4 - w11);                                  \
    store(output + 5 * stride, -w3 - (kWeight2 * w10 + kWeight2 * w8));    \
    store(output + 6 * stride, -w12);                                      \
    store(output + 7 * stride, w3 + (-kWeight2 * w10 - kWeight2 * w8));    \
  }

#define GEN_FFT_16(ret, suffix, T, T_VEC, load, store, constant)              \
  ret aom_fft1d_16_##suffix(const T *input, T *output, int stride) {          \
    const T_VEC kWeight2 = constant(0.707107f);                               \
    const T_VEC kWeight3 = constant(0.92388f);                                \
    const T_VEC kWeight4 = constant(0.382683f);                               \
    const T_VEC i0 = load(input + 0 * stride);                                \
    const T_VEC i1 = load(input + 1 * stride);                                \
    const T_VEC i2 = load(input + 2 * stride);                                \
    const T_VEC i3 = load(input + 3 * stride);                                \
    const T_VEC i4 = load(input + 4 * stride);                                \
    const T_VEC i5 = load(input + 5 * stride);                                \
    const T_VEC i6 = load(input + 6 * stride);                                \
    const T_VEC i7 = load(input + 7 * stride);                                \
    const T_VEC i8 = load(input + 8 * stride);                                \
    const T_VEC i9 = load(input + 9 * stride);                                \
    const T_VEC i10 = load(input + 10 * stride);                              \
    const T_VEC i11 = load(input + 11 * stride);                              \
    const T_VEC i12 = load(input + 12 * stride);                              \
    const T_VEC i13 = load(input + 13 * stride);                              \
    const T_VEC i14 = load(input + 14 * stride);                              \
    const T_VEC i15 = load(input + 15 * stride);                              \
    const T_VEC w0 = i0 + i8;                                                 \
    const T_VEC w1 = i0 - i8;                                                 \
    const T_VEC w2 = i4 + i12;                                                \
    const T_VEC w3 = i4 - i12;                                                \
    const T_VEC w4 = w0 + w2;                                                 \
    const T_VEC w5 = w0 - w2;                                                 \
    const T_VEC w7 = i2 + i10;                                                \
    const T_VEC w8 = i2 - i10;                                                \
    const T_VEC w9 = i6 + i14;                                                \
    const T_VEC w10 = i6 - i14;                                               \
    const T_VEC w11 = w7 + w9;                                                \
    const T_VEC w12 = w7 - w9;                                                \
    const T_VEC w14 = w4 + w11;                                               \
    const T_VEC w15 = w4 - w11;                                               \
    const T_VEC w16[2] = { (T_VEC)(w1 + (kWeight2 * w8 - kWeight2 * w10)),    \
                           (T_VEC)(-w3 - (kWeight2 * w10 + kWeight2 * w8)) }; \
    const T_VEC w18[2] = { (T_VEC)(w1 + (-kWeight2 * w8 + kWeight2 * w10)),   \
                           (T_VEC)(w3 + (-kWeight2 * w10 - kWeight2 * w8)) }; \
    const T_VEC w19 = i1 + i9;                                                \
    const T_VEC w20 = i1 - i9;                                                \
    const T_VEC w21 = i5 + i13;                                               \
    const T_VEC w22 = i5 - i13;                                               \
    const T_VEC w23 = w19 + w21;                                              \
    const T_VEC w24 = w19 - w21;                                              \
    const T_VEC w26 = i3 + i11;                                               \
    const T_VEC w27 = i3 - i11;                                               \
    const T_VEC w28 = i7 + i15;                                               \
    const T_VEC w29 = i7 - i15;                                               \
    const T_VEC w30 = w26 + w28;                                              \
    const T_VEC w31 = w26 - w28;                                              \
    const T_VEC w33 = w23 + w30;                                              \
    const T_VEC w34 = w23 - w30;                                              \
    const T_VEC w35[2] = { (T_VEC)(w20 + (kWeight2 * w27 - kWeight2 * w29)),  \
                           (T_VEC)(-w22 +                                     \
                                   -(kWeight2 * w29 + kWeight2 * w27)) };     \
    const T_VEC w37[2] = { (T_VEC)(w20 + (-kWeight2 * w27 + kWeight2 * w29)), \
                           (T_VEC)(w22 +                                      \
                                   (-kWeight2 * w29 - kWeight2 * w27)) };     \
    store(output + 0 * stride, w14 + w33);                                    \
    store(output + 1 * stride,                                                \
          w16[0] + (kWeight3 * w35[0] + kWeight4 * w35[1]));                  \
    store(output + 2 * stride, w5 + (kWeight2 * w24 - kWeight2 * w31));       \
    store(output + 3 * stride,                                                \
          w18[0] + (kWeight4 * w37[0] + kWeight3 * w37[1]));                  \
    store(output + 4 * stride, w15);                                          \
    store(output + 5 * stride,                                                \
          w18[0] + (-kWeight4 * w37[0] - kWeight3 * w37[1]));                 \
    store(output + 6 * stride, w5 + (-kWeight2 * w24 + kWeight2 * w31));      \
    store(output + 7 * stride,                                                \
          w16[0] + (-kWeight3 * w35[0] - kWeight4 * w35[1]));                 \
    store(output + 8 * stride, w14 - w33);                                    \
    store(output + 9 * stride,                                                \
          w16[1] + (kWeight3 * w35[1] - kWeight4 * w35[0]));                  \
    store(output + 10 * stride, -w12 - (kWeight2 * w31 + kWeight2 * w24));    \
    store(output + 11 * stride,                                               \
          w18[1] + (kWeight4 * w37[1] - kWeight3 * w37[0]));                  \
    store(output + 12 * stride, -w34);                                        \
    store(output + 13 * stride,                                               \
          -w18[1] - (-kWeight4 * w37[1] + kWeight3 * w37[0]));                \
    store(output + 14 * stride, w12 + (-kWeight2 * w31 - kWeight2 * w24));    \
    store(output + 15 * stride,                                               \
          -w16[1] - (-kWeight3 * w35[1] + kWeight4 * w35[0]));                \
  }

#define GEN_FFT_32(ret, suffix, T, T_VEC, load, store, constant)              \
  ret aom_fft1d_32_##suffix(const T *input, T *output, int stride) {          \
    const T_VEC kWeight2 = constant(0.707107f);                               \
    const T_VEC kWeight3 = constant(0.92388f);                                \
    const T_VEC kWeight4 = constant(0.382683f);                               \
    const T_VEC kWeight5 = constant(0.980785f);                               \
    const T_VEC kWeight6 = constant(0.19509f);                                \
    const T_VEC kWeight7 = constant(0.83147f);                                \
    const T_VEC kWeight8 = constant(0.55557f);                                \
    const T_VEC i0 = load(input + 0 * stride);                                \
    const T_VEC i1 = load(input + 1 * stride);                                \
    const T_VEC i2 = load(input + 2 * stride);                                \
    const T_VEC i3 = load(input + 3 * stride);                                \
    const T_VEC i4 = load(input + 4 * stride);                                \
    const T_VEC i5 = load(input + 5 * stride);                                \
    const T_VEC i6 = load(input + 6 * stride);                                \
    const T_VEC i7 = load(input + 7 * stride);                                \
    const T_VEC i8 = load(input + 8 * stride);                                \
    const T_VEC i9 = load(input + 9 * stride);                                \
    const T_VEC i10 = load(input + 10 * stride);                              \
    const T_VEC i11 = load(input + 11 * stride);                              \
    const T_VEC i12 = load(input + 12 * stride);                              \
    const T_VEC i13 = load(input + 13 * stride);                              \
    const T_VEC i14 = load(input + 14 * stride);                              \
    const T_VEC i15 = load(input + 15 * stride);                              \
    const T_VEC i16 = load(input + 16 * stride);                              \
    const T_VEC i17 = load(input + 17 * stride);                              \
    const T_VEC i18 = load(input + 18 * stride);                              \
    const T_VEC i19 = load(input + 19 * stride);                              \
    const T_VEC i20 = load(input + 20 * stride);                              \
    const T_VEC i21 = load(input + 21 * stride);                              \
    const T_VEC i22 = load(input + 22 * stride);                              \
    const T_VEC i23 = load(input + 23 * stride);                              \
    const T_VEC i24 = load(input + 24 * stride);                              \
    const T_VEC i25 = load(input + 25 * stride);                              \
    const T_VEC i26 = load(input + 26 * stride);                              \
    const T_VEC i27 = load(input + 27 * stride);                              \
    const T_VEC i28 = load(input + 28 * stride);                              \
    const T_VEC i29 = load(input + 29 * stride);                              \
    const T_VEC i30 = load(input + 30 * stride);                              \
    const T_VEC i31 = load(input + 31 * stride);                              \
    const T_VEC w0 = i0 + i16;                                                \
    const T_VEC w1 = i0 - i16;                                                \
    const T_VEC w2 = i8 + i24;                                                \
    const T_VEC w3 = i8 - i24;                                                \
    const T_VEC w4 = w0 + w2;                                                 \
    const T_VEC w5 = w0 - w2;                                                 \
    const T_VEC w7 = i4 + i20;                                                \
    const T_VEC w8 = i4 - i20;                                                \
    const T_VEC w9 = i12 + i28;                                               \
    const T_VEC w10 = i12 - i28;                                              \
    const T_VEC w11 = w7 + w9;                                                \
    const T_VEC w12 = w7 - w9;                                                \
    const T_VEC w14 = w4 + w11;                                               \
    const T_VEC w15 = w4 - w11;                                               \
    const T_VEC w16[2] = { (T_VEC)(w1 + (kWeight2 * w8 - kWeight2 * w10)),    \
                           (T_VEC)(-w3 - (kWeight2 * w10 + kWeight2 * w8)) }; \
    const T_VEC w18[2] = { (T_VEC)(w1 + (-kWeight2 * w8 + kWeight2 * w10)),   \
                           (T_VEC)(w3 + (-kWeight2 * w10 - kWeight2 * w8)) }; \
    const T_VEC w19 = i2 + i18;                                               \
    const T_VEC w20 = i2 - i18;                                               \
    const T_VEC w21 = i10 + i26;                                              \
    const T_VEC w22 = i10 - i26;                                              \
    const T_VEC w23 = w19 + w21;                                              \
    const T_VEC w24 = w19 - w21;                                              \
    const T_VEC w26 = i6 + i22;                                               \
    const T_VEC w27 = i6 - i22;                                               \
    const T_VEC w28 = i14 + i30;                                              \
    const T_VEC w29 = i14 - i30;                                              \
    const T_VEC w30 = w26 + w28;                                              \
    const T_VEC w31 = w26 - w28;                                              \
    const T_VEC w33 = w23 + w30;                                              \
    const T_VEC w34 = w23 - w30;                                              \
    const T_VEC w35[2] = { (T_VEC)(w20 + (kWeight2 * w27 - kWeight2 * w29)),  \
                           (T_VEC)(-w22 +                                     \
                                   -(kWeight2 * w29 + kWeight2 * w27)) };     \
    const T_VEC w37[2] = { (T_VEC)(w20 + (-kWeight2 * w27 + kWeight2 * w29)), \
                           (T_VEC)(w22 +                                      \
                                   (-kWeight2 * w29 - kWeight2 * w27)) };     \
    const T_VEC w38 = w14 + w33;                                              \
    const T_VEC w39 = w14 - w33;                                              \
    const T_VEC w40[2] = {                                                    \
      (T_VEC)(w16[0] + (kWeight3 * w35[0] + kWeight4 * w35[1])),              \
      (T_VEC)(w16[1] + (kWeight3 * w35[1] - kWeight4 * w35[0]))               \
    };                                                                        \
    const T_VEC w41[2] = { (T_VEC)(w5 + (kWeight2 * w24 - kWeight2 * w31)),   \
                           (T_VEC)(-w12 +                                     \
                                   -(kWeight2 * w31 + kWeight2 * w24)) };     \
    const T_VEC w42[2] = {                                                    \
      (T_VEC)(w18[0] + (kWeight4 * w37[0] + kWeight3 * w37[1])),              \
      (T_VEC)(w18[1] + (kWeight4 * w37[1] - kWeight3 * w37[0]))               \
    };                                                                        \
    const T_VEC w44[2] = {                                                    \
      (T_VEC)(w18[0] + (-kWeight4 * w37[0] - kWeight3 * w37[1])),             \
      (T_VEC)(-w18[1] - (-kWeight4 * w37[1] + kWeight3 * w37[0]))             \
    };                                                                        \
    const T_VEC w45[2] = { (T_VEC)(w5 + (-kWeight2 * w24 + kWeight2 * w31)),  \
                           (T_VEC)(w12 +                                      \
                                   (-kWeight2 * w31 - kWeight2 * w24)) };     \
    const T_VEC w46[2] = {                                                    \
      (T_VEC)(w16[0] + (-kWeight3 * w35[0] - kWeight4 * w35[1])),             \
      (T_VEC)(-w16[1] - (-kWeight3 * w35[1] + kWeight4 * w35[0]))             \
    };                                                                        \
    const T_VEC w47 = i1 + i17;                                               \
    const T_VEC w48 = i1 - i17;                                               \
    const T_VEC w49 = i9 + i25;                                               \
    const T_VEC w50 = i9 - i25;                                               \
    const T_VEC w51 = w47 + w49;                                              \
    const T_VEC w52 = w47 - w49;                                              \
    const T_VEC w54 = i5 + i21;                                               \
    const T_VEC w55 = i5 - i21;                                               \
    const T_VEC w56 = i13 + i29;                                              \
    const T_VEC w57 = i13 - i29;                                              \
    const T_VEC w58 = w54 + w56;                                              \
    const T_VEC w59 = w54 - w56;                                              \
    const T_VEC w61 = w51 + w58;                                              \
    const T_VEC w62 = w51 - w58;                                              \
    const T_VEC w63[2] = { (T_VEC)(w48 + (kWeight2 * w55 - kWeight2 * w57)),  \
                           (T_VEC)(-w50 +                                     \
                                   -(kWeight2 * w57 + kWeight2 * w55)) };     \
    const T_VEC w65[2] = { (T_VEC)(w48 + (-kWeight2 * w55 + kWeight2 * w57)), \
                           (T_VEC)(w50 +                                      \
                                   (-kWeight2 * w57 - kWeight2 * w55)) };     \
    const T_VEC w66 = i3 + i19;                                               \
    const T_VEC w67 = i3 - i19;                                               \
    const T_VEC w68 = i11 + i27;                                              \
    const T_VEC w69 = i11 - i27;                                              \
    const T_VEC w70 = w66 + w68;                                              \
    const T_VEC w71 = w66 - w68;                                              \
    const T_VEC w73 = i7 + i23;                                               \
    const T_VEC w74 = i7 - i23;                                               \
    const T_VEC w75 = i15 + i31;                                              \
    const T_VEC w76 = i15 - i31;                                              \
    const T_VEC w77 = w73 + w75;                                              \
    const T_VEC w78 = w73 - w75;                                              \
    const T_VEC w80 = w70 + w77;                                              \
    const T_VEC w81 = w70 - w77;                                              \
    const T_VEC w82[2] = { (T_VEC)(w67 + (kWeight2 * w74 - kWeight2 * w76)),  \
                           (T_VEC)(-w69 +                                     \
                                   -(kWeight2 * w76 + kWeight2 * w74)) };     \
    const T_VEC w84[2] = { (T_VEC)(w67 + (-kWeight2 * w74 + kWeight2 * w76)), \
                           (T_VEC)(w69 +                                      \
                                   (-kWeight2 * w76 - kWeight2 * w74)) };     \
    const T_VEC w85 = w61 + w80;                                              \
    const T_VEC w86 = w61 - w80;                                              \
    const T_VEC w87[2] = {                                                    \
      (T_VEC)(w63[0] + (kWeight3 * w82[0] + kWeight4 * w82[1])),              \
      (T_VEC)(w63[1] + (kWeight3 * w82[1] - kWeight4 * w82[0]))               \
    };                                                                        \
    const T_VEC w88[2] = { (T_VEC)(w52 + (kWeight2 * w71 - kWeight2 * w78)),  \
                           (T_VEC)(-w59 +                                     \
                                   -(kWeight2 * w78 + kWeight2 * w71)) };     \
    const T_VEC w89[2] = {                                                    \
      (T_VEC)(w65[0] + (kWeight4 * w84[0] + kWeight3 * w84[1])),              \
      (T_VEC)(w65[1] + (kWeight4 * w84[1] - kWeight3 * w84[0]))               \
    };                                                                        \
    const T_VEC w91[2] = {                                                    \
      (T_VEC)(w65[0] + (-kWeight4 * w84[0] - kWeight3 * w84[1])),             \
      (T_VEC)(-w65[1] - (-kWeight4 * w84[1] + kWeight3 * w84[0]))             \
    };                                                                        \
    const T_VEC w92[2] = { (T_VEC)(w52 + (-kWeight2 * w71 + kWeight2 * w78)), \
                           (T_VEC)(w59 +                                      \
                                   (-kWeight2 * w78 - kWeight2 * w71)) };     \
    const T_VEC w93[2] = {                                                    \
      (T_VEC)(w63[0] + (-kWeight3 * w82[0] - kWeight4 * w82[1])),             \
      (T_VEC)(-w63[1] - (-kWeight3 * w82[1] + kWeight4 * w82[0]))             \
    };                                                                        \
    store(output + 0 * stride, w38 + w85);                                    \
    store(output + 1 * stride,                                                \
          w40[0] + (kWeight5 * w87[0] + kWeight6 * w87[1]));                  \
    store(output + 2 * stride,                                                \
          w41[0] + (kWeight3 * w88[0] + kWeight4 * w88[1]));                  \
    store(output + 3 * stride,                                                \
          w42[0] + (kWeight7 * w89[0] + kWeight8 * w89[1]));                  \
    store(output + 4 * stride, w15 + (kWeight2 * w62 - kWeight2 * w81));      \
    store(output + 5 * stride,                                                \
          w44[0] + (kWeight8 * w91[0] + kWeight7 * w91[1]));                  \
    store(output + 6 * stride,                                                \
          w45[0] + (kWeight4 * w92[0] + kWeight3 * w92[1]));                  \
    store(output + 7 * stride,                                                \
          w46[0] + (kWeight6 * w93[0] + kWeight5 * w93[1]));                  \
    store(output + 8 * stride, w39);                                          \
    store(output + 9 * stride,                                                \
          w46[0] + (-kWeight6 * w93[0] - kWeight5 * w93[1]));                 \
    store(output + 10 * stride,                                               \
          w45[0] + (-kWeight4 * w92[0] - kWeight3 * w92[1]));                 \
    store(output + 11 * stride,                                               \
          w44[0] + (-kWeight8 * w91[0] - kWeight7 * w91[1]));                 \
    store(output + 12 * stride, w15 + (-kWeight2 * w62 + kWeight2 * w81));    \
    store(output + 13 * stride,                                               \
          w42[0] + (-kWeight7 * w89[0] - kWeight8 * w89[1]));                 \
    store(output + 14 * stride,                                               \
          w41[0] + (-kWeight3 * w88[0] - kWeight4 * w88[1]));                 \
    store(output + 15 * stride,                                               \
          w40[0] + (-kWeight5 * w87[0] - kWeight6 * w87[1]));                 \
    store(output + 16 * stride, w38 - w85);                                   \
    store(output + 17 * stride,                                               \
          w40[1] + (kWeight5 * w87[1] - kWeight6 * w87[0]));                  \
    store(output + 18 * stride,                                               \
          w41[1] + (kWeight3 * w88[1] - kWeight4 * w88[0]));                  \
    store(output + 19 * stride,                                               \
          w42[1] + (kWeight7 * w89[1] - kWeight8 * w89[0]));                  \
    store(output + 20 * stride, -w34 - (kWeight2 * w81 + kWeight2 * w62));    \
    store(output + 21 * stride,                                               \
          w44[1] + (kWeight8 * w91[1] - kWeight7 * w91[0]));                  \
    store(output + 22 * stride,                                               \
          w45[1] + (kWeight4 * w92[1] - kWeight3 * w92[0]));                  \
    store(output + 23 * stride,                                               \
          w46[1] + (kWeight6 * w93[1] - kWeight5 * w93[0]));                  \
    store(output + 24 * stride, -w86);                                        \
    store(output + 25 * stride,                                               \
          -w46[1] - (-kWeight6 * w93[1] + kWeight5 * w93[0]));                \
    store(output + 26 * stride,                                               \
          -w45[1] - (-kWeight4 * w92[1] + kWeight3 * w92[0]));                \
    store(output + 27 * stride,                                               \
          -w44[1] - (-kWeight8 * w91[1] + kWeight7 * w91[0]));                \
    store(output + 28 * stride, w34 + (-kWeight2 * w81 - kWeight2 * w62));    \
    store(output + 29 * stride,                                               \
          -w42[1] - (-kWeight7 * w89[1] + kWeight8 * w89[0]));                \
    store(output + 30 * stride,                                               \
          -w41[1] - (-kWeight3 * w88[1] + kWeight4 * w88[0]));                \
    store(output + 31 * stride,                                               \
          -w40[1] - (-kWeight5 * w87[1] + kWeight6 * w87[0]));                \
  }

#define GEN_IFFT_2(ret, suffix, T, T_VEC, load, store)               \
  ret aom_ifft1d_2_##suffix(const T *input, T *output, int stride) { \
    const T_VEC i0 = load(input + 0 * stride);                       \
    const T_VEC i1 = load(input + 1 * stride);                       \
    store(output + 0 * stride, i0 + i1);                             \
    store(output + 1 * stride, i0 - i1);                             \
  }

#define GEN_IFFT_4(ret, suffix, T, T_VEC, load, store)               \
  ret aom_ifft1d_4_##suffix(const T *input, T *output, int stride) { \
    const T_VEC i0 = load(input + 0 * stride);                       \
    const T_VEC i1 = load(input + 1 * stride);                       \
    const T_VEC i2 = load(input + 2 * stride);                       \
    const T_VEC i3 = load(input + 3 * stride);                       \
    const T_VEC w2 = i0 + i2;                                        \
    const T_VEC w3 = i0 - i2;                                        \
    const T_VEC w4[2] = { (T_VEC)(i1 + i1), (T_VEC)(-i3 + i3) };     \
    const T_VEC w5[2] = { (T_VEC)(i1 - i1), (T_VEC)(-i3 - i3) };     \
    store(output + 0 * stride, w2 + w4[0]);                          \
    store(output + 1 * stride, w3 + w5[1]);                          \
    store(output + 2 * stride, w2 - w4[0]);                          \
    store(output + 3 * stride, w3 - w5[1]);                          \
  }

#define GEN_IFFT_8(ret, suffix, T, T_VEC, load, store, constant)     \
  ret aom_ifft1d_8_##suffix(const T *input, T *output, int stride) { \
    const T_VEC kWeight2 = constant(0.707107f);                      \
    const T_VEC i0 = load(input + 0 * stride);                       \
    const T_VEC i1 = load(input + 1 * stride);                       \
    const T_VEC i2 = load(input + 2 * stride);                       \
    const T_VEC i3 = load(input + 3 * stride);                       \
    const T_VEC i4 = load(input + 4 * stride);                       \
    const T_VEC i5 = load(input + 5 * stride);                       \
    const T_VEC i6 = load(input + 6 * stride);                       \
    const T_VEC i7 = load(input + 7 * stride);                       \
    const T_VEC w6 = i0 + i4;                                        \
    const T_VEC w7 = i0 - i4;                                        \
    const T_VEC w8[2] = { (T_VEC)(i2 + i2), (T_VEC)(-i6 + i6) };     \
    const T_VEC w9[2] = { (T_VEC)(i2 - i2), (T_VEC)(-i6 - i6) };     \
    const T_VEC w10[2] = { (T_VEC)(w6 + w8[0]), (T_VEC)(w8[1]) };    \
    const T_VEC w11[2] = { (T_VEC)(w6 - w8[0]), (T_VEC)(-w8[1]) };   \
    const T_VEC w12[2] = { (T_VEC)(w7 + w9[1]), (T_VEC)(-w9[0]) };   \
    const T_VEC w13[2] = { (T_VEC)(w7 - w9[1]), (T_VEC)(w9[0]) };    \
    const T_VEC w14[2] = { (T_VEC)(i1 + i3), (T_VEC)(-i5 + i7) };    \
    const T_VEC w15[2] = { (T_VEC)(i1 - i3), (T_VEC)(-i5 - i7) };    \
    const T_VEC w16[2] = { (T_VEC)(i3 + i1), (T_VEC)(-i7 + i5) };    \
    const T_VEC w17[2] = { (T_VEC)(i3 - i1), (T_VEC)(-i7 - i5) };    \
    const T_VEC w18[2] = { (T_VEC)(w14[0] + w16[0]),                 \
                           (T_VEC)(w14[1] + w16[1]) };               \
    const T_VEC w19[2] = { (T_VEC)(w14[0] - w16[0]),                 \
                           (T_VEC)(w14[1] - w16[1]) };               \
    const T_VEC w20[2] = { (T_VEC)(w15[0] + w17[1]),                 \
                           (T_VEC)(w15[1] - w17[0]) };               \
    const T_VEC w21[2] = { (T_VEC)(w15[0] - w17[1]),                 \
                           (T_VEC)(w15[1] + w17[0]) };               \
    store(output + 0 * stride, w10[0] + w18[0]);                     \
    store(output + 1 * stride,                                       \
          w12[0] + (kWeight2 * w20[0] + kWeight2 * w20[1]));         \
    store(output + 2 * stride, w11[0] + w19[1]);                     \
    store(output + 3 * stride,                                       \
          w13[0] + (-kWeight2 * w21[0] + kWeight2 * w21[1]));        \
    store(output + 4 * stride, w10[0] - w18[0]);                     \
    store(output + 5 * stride,                                       \
          w12[0] + (-kWeight2 * w20[0] - kWeight2 * w20[1]));        \
    store(output + 6 * stride, w11[0] - w19[1]);                     \
    store(output + 7 * stride,                                       \
          w13[0] + (kWeight2 * w21[0] - kWeight2 * w21[1]));         \
  }

#define GEN_IFFT_16(ret, suffix, T, T_VEC, load, store, constant)     \
  ret aom_ifft1d_16_##suffix(const T *input, T *output, int stride) { \
    const T_VEC kWeight2 = constant(0.707107f);                       \
    const T_VEC kWeight3 = constant(0.92388f);                        \
    const T_VEC kWeight4 = constant(0.382683f);                       \
    const T_VEC i0 = load(input + 0 * stride);                        \
    const T_VEC i1 = load(input + 1 * stride);                        \
    const T_VEC i2 = load(input + 2 * stride);                        \
    const T_VEC i3 = load(input + 3 * stride);                        \
    const T_VEC i4 = load(input + 4 * stride);                        \
    const T_VEC i5 = load(input + 5 * stride);                        \
    const T_VEC i6 = load(input + 6 * stride);                        \
    const T_VEC i7 = load(input + 7 * stride);                        \
    const T_VEC i8 = load(input + 8 * stride);                        \
    const T_VEC i9 = load(input + 9 * stride);                        \
    const T_VEC i10 = load(input + 10 * stride);                      \
    const T_VEC i11 = load(input + 11 * stride);                      \
    const T_VEC i12 = load(input + 12 * stride);                      \
    const T_VEC i13 = load(input + 13 * stride);                      \
    const T_VEC i14 = load(input + 14 * stride);                      \
    const T_VEC i15 = load(input + 15 * stride);                      \
    const T_VEC w14 = i0 + i8;                                        \
    const T_VEC w15 = i0 - i8;                                        \
    const T_VEC w16[2] = { (T_VEC)(i4 + i4), (T_VEC)(-i12 + i12) };   \
    const T_VEC w17[2] = { (T_VEC)(i4 - i4), (T_VEC)(-i12 - i12) };   \
    const T_VEC w18[2] = { (T_VEC)(w14 + w16[0]), (T_VEC)(w16[1]) };  \
    const T_VEC w19[2] = { (T_VEC)(w14 - w16[0]), (T_VEC)(-w16[1]) }; \
    const T_VEC w20[2] = { (T_VEC)(w15 + w17[1]), (T_VEC)(-w17[0]) }; \
    const T_VEC w21[2] = { (T_VEC)(w15 - w17[1]), (T_VEC)(w17[0]) };  \
    const T_VEC w22[2] = { (T_VEC)(i2 + i6), (T_VEC)(-i10 + i14) };   \
    const T_VEC w23[2] = { (T_VEC)(i2 - i6), (T_VEC)(-i10 - i14) };   \
    const T_VEC w24[2] = { (T_VEC)(i6 + i2), (T_VEC)(-i14 + i10) };   \
    const T_VEC w25[2] = { (T_VEC)(i6 - i2), (T_VEC)(-i14 - i10) };   \
    const T_VEC w26[2] = { (T_VEC)(w22[0] + w24[0]),                  \
                           (T_VEC)(w22[1] + w24[1]) };                \
    const T_VEC w27[2] = { (T_VEC)(w22[0] - w24[0]),                  \
                           (T_VEC)(w22[1] - w24[1]) };                \
    const T_VEC w28[2] = { (T_VEC)(w23[0] + w25[1]),                  \
                           (T_VEC)(w23[1] - w25[0]) };                \
    const T_VEC w29[2] = { (T_VEC)(w23[0] - w25[1]),                  \
                           (T_VEC)(w23[1] + w25[0]) };                \
    const T_VEC w30[2] = { (T_VEC)(w18[0] + w26[0]),                  \
                           (T_VEC)(w18[1] + w26[1]) };                \
    const T_VEC w31[2] = { (T_VEC)(w18[0] - w26[0]),                  \
                           (T_VEC)(w18[1] - w26[1]) };                \
    const T_VEC w32[2] = {                                            \
      (T_VEC)(w20[0] + (kWeight2 * w28[0] + kWeight2 * w28[1])),      \
      (T_VEC)(w20[1] + (kWeight2 * w28[1] - kWeight2 * w28[0]))       \
    };                                                                \
    const T_VEC w33[2] = {                                            \
      (T_VEC)(w20[0] + (-kWeight2 * w28[0] - kWeight2 * w28[1])),     \
      (T_VEC)(w20[1] + (-kWeight2 * w28[1] + kWeight2 * w28[0]))      \
    };                                                                \
    const T_VEC w34[2] = { (T_VEC)(w19[0] + w27[1]),                  \
                           (T_VEC)(w19[1] - w27[0]) };                \
    const T_VEC w35[2] = { (T_VEC)(w19[0] - w27[1]),                  \
                           (T_VEC)(w19[1] + w27[0]) };                \
    const T_VEC w36[2] = {                                            \
      (T_VEC)(w21[0] + (-kWeight2 * w29[0] + kWeight2 * w29[1])),     \
      (T_VEC)(w21[1] + (-kWeight2 * w29[1] - kWeight2 * w29[0]))      \
    };                                                                \
    const T_VEC w37[2] = {                                            \
      (T_VEC)(w21[0] + (kWeight2 * w29[0] - kWeight2 * w29[1])),      \
      (T_VEC)(w21[1] + (kWeight2 * w29[1] + kWeight2 * w29[0]))       \
    };                                                                \
    const T_VEC w38[2] = { (T_VEC)(i1 + i7), (T_VEC)(-i9 + i15) };    \
    const T_VEC w39[2] = { (T_VEC)(i1 - i7), (T_VEC)(-i9 - i15) };    \
    const T_VEC w40[2] = { (T_VEC)(i5 + i3), (T_VEC)(-i13 + i11) };   \
    const T_VEC w41[2] = { (T_VEC)(i5 - i3), (T_VEC)(-i13 - i11) };   \
    const T_VEC w42[2] = { (T_VEC)(w38[0] + w40[0]),                  \
                           (T_VEC)(w38[1] + w40[1]) };                \
    const T_VEC w43[2] = { (T_VEC)(w38[0] - w40[0]),                  \
                           (T_VEC)(w38[1] - w40[1]) };                \
    const T_VEC w44[2] = { (T_VEC)(w39[0] + w41[1]),                  \
                           (T_VEC)(w39[1] - w41[0]) };                \
    const T_VEC w45[2] = { (T_VEC)(w39[0] - w41[1]),                  \
                           (T_VEC)(w39[1] + w41[0]) };                \
    const T_VEC w46[2] = { (T_VEC)(i3 + i5), (T_VEC)(-i11 + i13) };   \
    const T_VEC w47[2] = { (T_VEC)(i3 - i5), (T_VEC)(-i11 - i13) };   \
    const T_VEC w48[2] = { (T_VEC)(i7 + i1), (T_VEC)(-i15 + i9) };    \
    const T_VEC w49[2] = { (T_VEC)(i7 - i1), (T_VEC)(-i15 - i9) };    \
    const T_VEC w50[2] = { (T_VEC)(w46[0] + w48[0]),                  \
                           (T_VEC)(w46[1] + w48[1]) };                \
    const T_VEC w51[2] = { (T_VEC)(w46[0] - w48[0]),                  \
                           (T_VEC)(w46[1] - w48[1]) };                \
    const T_VEC w52[2] = { (T_VEC)(w47[0] + w49[1]),                  \
                           (T_VEC)(w47[1] - w49[0]) };                \
    const T_VEC w53[2] = { (T_VEC)(w47[0] - w49[1]),                  \
                           (T_VEC)(w47[1] + w49[0]) };                \
    const T_VEC w54[2] = { (T_VEC)(w42[0] + w50[0]),                  \
                           (T_VEC)(w42[1] + w50[1]) };                \
    const T_VEC w55[2] = { (T_VEC)(w42[0] - w50[0]),                  \
                           (T_VEC)(w42[1] - w50[1]) };                \
    const T_VEC w56[2] = {                                            \
      (T_VEC)(w44[0] + (kWeight2 * w52[0] + kWeight2 * w52[1])),      \
      (T_VEC)(w44[1] + (kWeight2 * w52[1] - kWeight2 * w52[0]))       \
    };                                                                \
    const T_VEC w57[2] = {                                            \
      (T_VEC)(w44[0] + (-kWeight2 * w52[0] - kWeight2 * w52[1])),     \
      (T_VEC)(w44[1] + (-kWeight2 * w52[1] + kWeight2 * w52[0]))      \
    };                                                                \
    const T_VEC w58[2] = { (T_VEC)(w43[0] + w51[1]),                  \
                           (T_VEC)(w43[1] - w51[0]) };                \
    const T_VEC w59[2] = { (T_VEC)(w43[0] - w51[1]),                  \
                           (T_VEC)(w43[1] + w51[0]) };                \
    const T_VEC w60[2] = {                                            \
      (T_VEC)(w45[0] + (-kWeight2 * w53[0] + kWeight2 * w53[1])),     \
      (T_VEC)(w45[1] + (-kWeight2 * w53[1] - kWeight2 * w53[0]))      \
    };                                                                \
    const T_VEC w61[2] = {                                            \
      (T_VEC)(w45[0] + (kWeight2 * w53[0] - kWeight2 * w53[1])),      \
      (T_VEC)(w45[1] + (kWeight2 * w53[1] + kWeight2 * w53[0]))       \
    };                                                                \
    store(output + 0 * stride, w30[0] + w54[0]);                      \
    store(output + 1 * stride,                                        \
          w32[0] + (kWeight3 * w56[0] + kWeight4 * w56[1]));          \
    store(output + 2 * stride,                                        \
          w34[0] + (kWeight2 * w58[0] + kWeight2 * w58[1]));          \
    store(output + 3 * stride,                                        \
          w36[0] + (kWeight4 * w60[0] + kWeight3 * w60[1]));          \
    store(output + 4 * stride, w31[0] + w55[1]);                      \
    store(output + 5 * stride,                                        \
          w33[0] + (-kWeight4 * w57[0] + kWeight3 * w57[1]));         \
    store(output + 6 * stride,                                        \
          w35[0] + (-kWeight2 * w59[0] + kWeight2 * w59[1]));         \
    store(output + 7 * stride,                                        \
          w37[0] + (-kWeight3 * w61[0] + kWeight4 * w61[1]));         \
    store(output + 8 * stride, w30[0] - w54[0]);                      \
    store(output + 9 * stride,                                        \
          w32[0] + (-kWeight3 * w56[0] - kWeight4 * w56[1]));         \
    store(output + 10 * stride,                                       \
          w34[0] + (-kWeight2 * w58[0] - kWeight2 * w58[1]));         \
    store(output + 11 * stride,                                       \
          w36[0] + (-kWeight4 * w60[0] - kWeight3 * w60[1]));         \
    store(output + 12 * stride, w31[0] - w55[1]);                     \
    store(output + 13 * stride,                                       \
          w33[0] + (kWeight4 * w57[0] - kWeight3 * w57[1]));          \
    store(output + 14 * stride,                                       \
          w35[0] + (kWeight2 * w59[0] - kWeight2 * w59[1]));          \
    store(output + 15 * stride,                                       \
          w37[0] + (kWeight3 * w61[0] - kWeight4 * w61[1]));          \
  }

#define GEN_IFFT_32(ret, suffix, T, T_VEC, load, store, constant)     \
  ret aom_ifft1d_32_##suffix(const T *input, T *output, int stride) { \
    const T_VEC kWeight2 = constant(0.707107f);                       \
    const T_VEC kWeight3 = constant(0.92388f);                        \
    const T_VEC kWeight4 = constant(0.382683f);                       \
    const T_VEC kWeight5 = constant(0.980785f);                       \
    const T_VEC kWeight6 = constant(0.19509f);                        \
    const T_VEC kWeight7 = constant(0.83147f);                        \
    const T_VEC kWeight8 = constant(0.55557f);                        \
    const T_VEC i0 = load(input + 0 * stride);                        \
    const T_VEC i1 = load(input + 1 * stride);                        \
    const T_VEC i2 = load(input + 2 * stride);                        \
    const T_VEC i3 = load(input + 3 * stride);                        \
    const T_VEC i4 = load(input + 4 * stride);                        \
    const T_VEC i5 = load(input + 5 * stride);                        \
    const T_VEC i6 = load(input + 6 * stride);                        \
    const T_VEC i7 = load(input + 7 * stride);                        \
    const T_VEC i8 = load(input + 8 * stride);                        \
    const T_VEC i9 = load(input + 9 * stride);                        \
    const T_VEC i10 = load(input + 10 * stride);                      \
    const T_VEC i11 = load(input + 11 * stride);                      \
    const T_VEC i12 = load(input + 12 * stride);                      \
    const T_VEC i13 = load(input + 13 * stride);                      \
    const T_VEC i14 = load(input + 14 * stride);                      \
    const T_VEC i15 = load(input + 15 * stride);                      \
    const T_VEC i16 = load(input + 16 * stride);                      \
    const T_VEC i17 = load(input + 17 * stride);                      \
    const T_VEC i18 = load(input + 18 * stride);                      \
    const T_VEC i19 = load(input + 19 * stride);                      \
    const T_VEC i20 = load(input + 20 * stride);                      \
    const T_VEC i21 = load(input + 21 * stride);                      \
    const T_VEC i22 = load(input + 22 * stride);                      \
    const T_VEC i23 = load(input + 23 * stride);                      \
    const T_VEC i24 = load(input + 24 * stride);                      \
    const T_VEC i25 = load(input + 25 * stride);                      \
    const T_VEC i26 = load(input + 26 * stride);                      \
    const T_VEC i27 = load(input + 27 * stride);                      \
    const T_VEC i28 = load(input + 28 * stride);                      \
    const T_VEC i29 = load(input + 29 * stride);                      \
    const T_VEC i30 = load(input + 30 * stride);                      \
    const T_VEC i31 = load(input + 31 * stride);                      \
    const T_VEC w30 = i0 + i16;                                       \
    const T_VEC w31 = i0 - i16;                                       \
    const T_VEC w32[2] = { (T_VEC)(i8 + i8), (T_VEC)(-i24 + i24) };   \
    const T_VEC w33[2] = { (T_VEC)(i8 - i8), (T_VEC)(-i24 - i24) };   \
    const T_VEC w34[2] = { (T_VEC)(w30 + w32[0]), (T_VEC)(w32[1]) };  \
    const T_VEC w35[2] = { (T_VEC)(w30 - w32[0]), (T_VEC)(-w32[1]) }; \
    const T_VEC w36[2] = { (T_VEC)(w31 + w33[1]), (T_VEC)(-w33[0]) }; \
    const T_VEC w37[2] = { (T_VEC)(w31 - w33[1]), (T_VEC)(w33[0]) };  \
    const T_VEC w38[2] = { (T_VEC)(i4 + i12), (T_VEC)(-i20 + i28) };  \
    const T_VEC w39[2] = { (T_VEC)(i4 - i12), (T_VEC)(-i20 - i28) };  \
    const T_VEC w40[2] = { (T_VEC)(i12 + i4), (T_VEC)(-i28 + i20) };  \
    const T_VEC w41[2] = { (T_VEC)(i12 - i4), (T_VEC)(-i28 - i20) };  \
    const T_VEC w42[2] = { (T_VEC)(w38[0] + w40[0]),                  \
                           (T_VEC)(w38[1] + w40[1]) };                \
    const T_VEC w43[2] = { (T_VEC)(w38[0] - w40[0]),                  \
                           (T_VEC)(w38[1] - w40[1]) };                \
    const T_VEC w44[2] = { (T_VEC)(w39[0] + w41[1]),                  \
                           (T_VEC)(w39[1] - w41[0]) };                \
    const T_VEC w45[2] = { (T_VEC)(w39[0] - w41[1]),                  \
                           (T_VEC)(w39[1] + w41[0]) };                \
    const T_VEC w46[2] = { (T_VEC)(w34[0] + w42[0]),                  \
                           (T_VEC)(w34[1] + w42[1]) };                \
    const T_VEC w47[2] = { (T_VEC)(w34[0] - w42[0]),                  \
                           (T_VEC)(w34[1] - w42[1]) };                \
    const T_VEC w48[2] = {                                            \
      (T_VEC)(w36[0] + (kWeight2 * w44[0] + kWeight2 * w44[1])),      \
      (T_VEC)(w36[1] + (kWeight2 * w44[1] - kWeight2 * w44[0]))       \
    };                                                                \
    const T_VEC w49[2] = {                                            \
      (T_VEC)(w36[0] + (-kWeight2 * w44[0] - kWeight2 * w44[1])),     \
      (T_VEC)(w36[1] + (-kWeight2 * w44[1] + kWeight2 * w44[0]))      \
    };                                                                \
    const T_VEC w50[2] = { (T_VEC)(w35[0] + w43[1]),                  \
                           (T_VEC)(w35[1] - w43[0]) };                \
    const T_VEC w51[2] = { (T_VEC)(w35[0] - w43[1]),                  \
                           (T_VEC)(w35[1] + w43[0]) };                \
    const T_VEC w52[2] = {                                            \
      (T_VEC)(w37[0] + (-kWeight2 * w45[0] + kWeight2 * w45[1])),     \
      (T_VEC)(w37[1] + (-kWeight2 * w45[1] - kWeight2 * w45[0]))      \
    };                                                                \
    const T_VEC w53[2] = {                                            \
      (T_VEC)(w37[0] + (kWeight2 * w45[0] - kWeight2 * w45[1])),      \
      (T_VEC)(w37[1] + (kWeight2 * w45[1] + kWeight2 * w45[0]))       \
    };                                                                \
    const T_VEC w54[2] = { (T_VEC)(i2 + i14), (T_VEC)(-i18 + i30) };  \
    const T_VEC w55[2] = { (T_VEC)(i2 - i14), (T_VEC)(-i18 - i30) };  \
    const T_VEC w56[2] = { (T_VEC)(i10 + i6), (T_VEC)(-i26 + i22) };  \
    const T_VEC w57[2] = { (T_VEC)(i10 - i6), (T_VEC)(-i26 - i22) };  \
    const T_VEC w58[2] = { (T_VEC)(w54[0] + w56[0]),                  \
                           (T_VEC)(w54[1] + w56[1]) };                \
    const T_VEC w59[2] = { (T_VEC)(w54[0] - w56[0]),                  \
                           (T_VEC)(w54[1] - w56[1]) };                \
    const T_VEC w60[2] = { (T_VEC)(w55[0] + w57[1]),                  \
                           (T_VEC)(w55[1] - w57[0]) };                \
    const T_VEC w61[2] = { (T_VEC)(w55[0] - w57[1]),                  \
                           (T_VEC)(w55[1] + w57[0]) };                \
    const T_VEC w62[2] = { (T_VEC)(i6 + i10), (T_VEC)(-i22 + i26) };  \
    const T_VEC w63[2] = { (T_VEC)(i6 - i10), (T_VEC)(-i22 - i26) };  \
    const T_VEC w64[2] = { (T_VEC)(i14 + i2), (T_VEC)(-i30 + i18) };  \
    const T_VEC w65[2] = { (T_VEC)(i14 - i2), (T_VEC)(-i30 - i18) };  \
    const T_VEC w66[2] = { (T_VEC)(w62[0] + w64[0]),                  \
                           (T_VEC)(w62[1] + w64[1]) };                \
    const T_VEC w67[2] = { (T_VEC)(w62[0] - w64[0]),                  \
                           (T_VEC)(w62[1] - w64[1]) };                \
    const T_VEC w68[2] = { (T_VEC)(w63[0] + w65[1]),                  \
                           (T_VEC)(w63[1] - w65[0]) };                \
    const T_VEC w69[2] = { (T_VEC)(w63[0] - w65[1]),                  \
                           (T_VEC)(w63[1] + w65[0]) };                \
    const T_VEC w70[2] = { (T_VEC)(w58[0] + w66[0]),                  \
                           (T_VEC)(w58[1] + w66[1]) };                \
    const T_VEC w71[2] = { (T_VEC)(w58[0] - w66[0]),                  \
                           (T_VEC)(w58[1] - w66[1]) };                \
    const T_VEC w72[2] = {                                            \
      (T_VEC)(w60[0] + (kWeight2 * w68[0] + kWeight2 * w68[1])),      \
      (T_VEC)(w60[1] + (kWeight2 * w68[1] - kWeight2 * w68[0]))       \
    };                                                                \
    const T_VEC w73[2] = {                                            \
      (T_VEC)(w60[0] + (-kWeight2 * w68[0] - kWeight2 * w68[1])),     \
      (T_VEC)(w60[1] + (-kWeight2 * w68[1] + kWeight2 * w68[0]))      \
    };                                                                \
    const T_VEC w74[2] = { (T_VEC)(w59[0] + w67[1]),                  \
                           (T_VEC)(w59[1] - w67[0]) };                \
    const T_VEC w75[2] = { (T_VEC)(w59[0] - w67[1]),                  \
                           (T_VEC)(w59[1] + w67[0]) };                \
    const T_VEC w76[2] = {                                            \
      (T_VEC)(w61[0] + (-kWeight2 * w69[0] + kWeight2 * w69[1])),     \
      (T_VEC)(w61[1] + (-kWeight2 * w69[1] - kWeight2 * w69[0]))      \
    };                                                                \
    const T_VEC w77[2] = {                                            \
      (T_VEC)(w61[0] + (kWeight2 * w69[0] - kWeight2 * w69[1])),      \
      (T_VEC)(w61[1] + (kWeight2 * w69[1] + kWeight2 * w69[0]))       \
    };                                                                \
    const T_VEC w78[2] = { (T_VEC)(w46[0] + w70[0]),                  \
                           (T_VEC)(w46[1] + w70[1]) };                \
    const T_VEC w79[2] = { (T_VEC)(w46[0] - w70[0]),                  \
                           (T_VEC)(w46[1] - w70[1]) };                \
    const T_VEC w80[2] = {                                            \
      (T_VEC)(w48[0] + (kWeight3 * w72[0] + kWeight4 * w72[1])),      \
      (T_VEC)(w48[1] + (kWeight3 * w72[1] - kWeight4 * w72[0]))       \
    };                                                                \
    const T_VEC w81[2] = {                                            \
      (T_VEC)(w48[0] + (-kWeight3 * w72[0] - kWeight4 * w72[1])),     \
      (T_VEC)(w48[1] + (-kWeight3 * w72[1] + kWeight4 * w72[0]))      \
    };                                                                \
    const T_VEC w82[2] = {                                            \
      (T_VEC)(w50[0] + (kWeight2 * w74[0] + kWeight2 * w74[1])),      \
      (T_VEC)(w50[1] + (kWeight2 * w74[1] - kWeight2 * w74[0]))       \
    };                                                                \
    const T_VEC w83[2] = {                                            \
      (T_VEC)(w50[0] + (-kWeight2 * w74[0] - kWeight2 * w74[1])),     \
      (T_VEC)(w50[1] + (-kWeight2 * w74[1] + kWeight2 * w74[0]))      \
    };                                                                \
    const T_VEC w84[2] = {                                            \
      (T_VEC)(w52[0] + (kWeight4 * w76[0] + kWeight3 * w76[1])),      \
      (T_VEC)(w52[1] + (kWeight4 * w76[1] - kWeight3 * w76[0]))       \
    };                                                                \
    const T_VEC w85[2] = {                                            \
      (T_VEC)(w52[0] + (-kWeight4 * w76[0] - kWeight3 * w76[1])),     \
      (T_VEC)(w52[1] + (-kWeight4 * w76[1] + kWeight3 * w76[0]))      \
    };                                                                \
    const T_VEC w86[2] = { (T_VEC)(w47[0] + w71[1]),                  \
                           (T_VEC)(w47[1] - w71[0]) };                \
    const T_VEC w87[2] = { (T_VEC)(w47[0] - w71[1]),                  \
                           (T_VEC)(w47[1] + w71[0]) };                \
    const T_VEC w88[2] = {                                            \
      (T_VEC)(w49[0] + (-kWeight4 * w73[0] + kWeight3 * w73[1])),     \
      (T_VEC)(w49[1] + (-kWeight4 * w73[1] - kWeight3 * w73[0]))      \
    };                                                                \
    const T_VEC w89[2] = {                                            \
      (T_VEC)(w49[0] + (kWeight4 * w73[0] - kWeight3 * w73[1])),      \
      (T_VEC)(w49[1] + (kWeight4 * w73[1] + kWeight3 * w73[0]))       \
    };                                                                \
    const T_VEC w90[2] = {                                            \
      (T_VEC)(w51[0] + (-kWeight2 * w75[0] + kWeight2 * w75[1])),     \
      (T_VEC)(w51[1] + (-kWeight2 * w75[1] - kWeight2 * w75[0]))      \
    };                                                                \
    const T_VEC w91[2] = {                                            \
      (T_VEC)(w51[0] + (kWeight2 * w75[0] - kWeight2 * w75[1])),      \
      (T_VEC)(w51[1] + (kWeight2 * w75[1] + kWeight2 * w75[0]))       \
    };                                                                \
    const T_VEC w92[2] = {                                            \
      (T_VEC)(w53[0] + (-kWeight3 * w77[0] + kWeight4 * w77[1])),     \
      (T_VEC)(w53[1] + (-kWeight3 * w77[1] - kWeight4 * w77[0]))      \
    };                                                                \
    const T_VEC w93[2] = {                                            \
      (T_VEC)(w53[0] + (kWeight3 * w77[0] - kWeight4 * w77[1])),      \
      (T_VEC)(w53[1] + (kWeight3 * w77[1] + kWeight4 * w77[0]))       \
    };                                                                \
    const T_VEC w94[2] = { (T_VEC)(i1 + i15), (T_VEC)(-i17 + i31) };  \
    const T_VEC w95[2] = { (T_VEC)(i1 - i15), (T_VEC)(-i17 - i31) };  \
    const T_VEC w96[2] = { (T_VEC)(i9 + i7), (T_VEC)(-i25 + i23) };   \
    const T_VEC w97[2] = { (T_VEC)(i9 - i7), (T_VEC)(-i25 - i23) };   \
    const T_VEC w98[2] = { (T_VEC)(w94[0] + w96[0]),                  \
                           (T_VEC)(w94[1] + w96[1]) };                \
    const T_VEC w99[2] = { (T_VEC)(w94[0] - w96[0]),                  \
                           (T_VEC)(w94[1] - w96[1]) };                \
    const T_VEC w100[2] = { (T_VEC)(w95[0] + w97[1]),                 \
                            (T_VEC)(w95[1] - w97[0]) };               \
    const T_VEC w101[2] = { (T_VEC)(w95[0] - w97[1]),                 \
                            (T_VEC)(w95[1] + w97[0]) };               \
    const T_VEC w102[2] = { (T_VEC)(i5 + i11), (T_VEC)(-i21 + i27) }; \
    const T_VEC w103[2] = { (T_VEC)(i5 - i11), (T_VEC)(-i21 - i27) }; \
    const T_VEC w104[2] = { (T_VEC)(i13 + i3), (T_VEC)(-i29 + i19) }; \
    const T_VEC w105[2] = { (T_VEC)(i13 - i3), (T_VEC)(-i29 - i19) }; \
    const T_VEC w106[2] = { (T_VEC)(w102[0] + w104[0]),               \
                            (T_VEC)(w102[1] + w104[1]) };             \
    const T_VEC w107[2] = { (T_VEC)(w102[0] - w104[0]),               \
                            (T_VEC)(w102[1] - w104[1]) };             \
    const T_VEC w108[2] = { (T_VEC)(w103[0] + w105[1]),               \
                            (T_VEC)(w103[1] - w105[0]) };             \
    const T_VEC w109[2] = { (T_VEC)(w103[0] - w105[1]),               \
                            (T_VEC)(w103[1] + w105[0]) };             \
    const T_VEC w110[2] = { (T_VEC)(w98[0] + w106[0]),                \
                            (T_VEC)(w98[1] + w106[1]) };              \
    const T_VEC w111[2] = { (T_VEC)(w98[0] - w106[0]),                \
                            (T_VEC)(w98[1] - w106[1]) };              \
    const T_VEC w112[2] = {                                           \
      (T_VEC)(w100[0] + (kWeight2 * w108[0] + kWeight2 * w108[1])),   \
      (T_VEC)(w100[1] + (kWeight2 * w108[1] - kWeight2 * w108[0]))    \
    };                                                                \
    const T_VEC w113[2] = {                                           \
      (T_VEC)(w100[0] + (-kWeight2 * w108[0] - kWeight2 * w108[1])),  \
      (T_VEC)(w100[1] + (-kWeight2 * w108[1] + kWeight2 * w108[0]))   \
    };                                                                \
    const T_VEC w114[2] = { (T_VEC)(w99[0] + w107[1]),                \
                            (T_VEC)(w99[1] - w107[0]) };              \
    const T_VEC w115[2] = { (T_VEC)(w99[0] - w107[1]),                \
                            (T_VEC)(w99[1] + w107[0]) };              \
    const T_VEC w116[2] = {                                           \
      (T_VEC)(w101[0] + (-kWeight2 * w109[0] + kWeight2 * w109[1])),  \
      (T_VEC)(w101[1] + (-kWeight2 * w109[1] - kWeight2 * w109[0]))   \
    };                                                                \
    const T_VEC w117[2] = {                                           \
      (T_VEC)(w101[0] + (kWeight2 * w109[0] - kWeight2 * w109[1])),   \
      (T_VEC)(w101[1] + (kWeight2 * w109[1] + kWeight2 * w109[0]))    \
    };                                                                \
    const T_VEC w118[2] = { (T_VEC)(i3 + i13), (T_VEC)(-i19 + i29) }; \
    const T_VEC w119[2] = { (T_VEC)(i3 - i13), (T_VEC)(-i19 - i29) }; \
    const T_VEC w120[2] = { (T_VEC)(i11 + i5), (T_VEC)(-i27 + i21) }; \
    const T_VEC w121[2] = { (T_VEC)(i11 - i5), (T_VEC)(-i27 - i21) }; \
    const T_VEC w122[2] = { (T_VEC)(w118[0] + w120[0]),               \
                            (T_VEC)(w118[1] + w120[1]) };             \
    const T_VEC w123[2] = { (T_VEC)(w118[0] - w120[0]),               \
                            (T_VEC)(w118[1] - w120[1]) };             \
    const T_VEC w124[2] = { (T_VEC)(w119[0] + w121[1]),               \
                            (T_VEC)(w119[1] - w121[0]) };             \
    const T_VEC w125[2] = { (T_VEC)(w119[0] - w121[1]),               \
                            (T_VEC)(w119[1] + w121[0]) };             \
    const T_VEC w126[2] = { (T_VEC)(i7 + i9), (T_VEC)(-i23 + i25) };  \
    const T_VEC w127[2] = { (T_VEC)(i7 - i9), (T_VEC)(-i23 - i25) };  \
    const T_VEC w128[2] = { (T_VEC)(i15 + i1), (T_VEC)(-i31 + i17) }; \
    const T_VEC w129[2] = { (T_VEC)(i15 - i1), (T_VEC)(-i31 - i17) }; \
    const T_VEC w130[2] = { (T_VEC)(w126[0] + w128[0]),               \
                            (T_VEC)(w126[1] + w128[1]) };             \
    const T_VEC w131[2] = { (T_VEC)(w126[0] - w128[0]),               \
                            (T_VEC)(w126[1] - w128[1]) };             \
    const T_VEC w132[2] = { (T_VEC)(w127[0] + w129[1]),               \
                            (T_VEC)(w127[1] - w129[0]) };             \
    const T_VEC w133[2] = { (T_VEC)(w127[0] - w129[1]),               \
                            (T_VEC)(w127[1] + w129[0]) };             \
    const T_VEC w134[2] = { (T_VEC)(w122[0] + w130[0]),               \
                            (T_VEC)(w122[1] + w130[1]) };             \
    const T_VEC w135[2] = { (T_VEC)(w122[0] - w130[0]),               \
                            (T_VEC)(w122[1] - w130[1]) };             \
    const T_VEC w136[2] = {                                           \
      (T_VEC)(w124[0] + (kWeight2 * w132[0] + kWeight2 * w132[1])),   \
      (T_VEC)(w124[1] + (kWeight2 * w132[1] - kWeight2 * w132[0]))    \
    };                                                                \
    const T_VEC w137[2] = {                                           \
      (T_VEC)(w124[0] + (-kWeight2 * w132[0] - kWeight2 * w132[1])),  \
      (T_VEC)(w124[1] + (-kWeight2 * w132[1] + kWeight2 * w132[0]))   \
    };                                                                \
    const T_VEC w138[2] = { (T_VEC)(w123[0] + w131[1]),               \
                            (T_VEC)(w123[1] - w131[0]) };             \
    const T_VEC w139[2] = { (T_VEC)(w123[0] - w131[1]),               \
                            (T_VEC)(w123[1] + w131[0]) };             \
    const T_VEC w140[2] = {                                           \
      (T_VEC)(w125[0] + (-kWeight2 * w133[0] + kWeight2 * w133[1])),  \
      (T_VEC)(w125[1] + (-kWeight2 * w133[1] - kWeight2 * w133[0]))   \
    };                                                                \
    const T_VEC w141[2] = {                                           \
      (T_VEC)(w125[0] + (kWeight2 * w133[0] - kWeight2 * w133[1])),   \
      (T_VEC)(w125[1] + (kWeight2 * w133[1] + kWeight2 * w133[0]))    \
    };                                                                \
    const T_VEC w142[2] = { (T_VEC)(w110[0] + w134[0]),               \
                            (T_VEC)(w110[1] + w134[1]) };             \
    const T_VEC w143[2] = { (T_VEC)(w110[0] - w134[0]),               \
                            (T_VEC)(w110[1] - w134[1]) };             \
    const T_VEC w144[2] = {                                           \
      (T_VEC)(w112[0] + (kWeight3 * w136[0] + kWeight4 * w136[1])),   \
      (T_VEC)(w112[1] + (kWeight3 * w136[1] - kWeight4 * w136[0]))    \
    };                                                                \
    const T_VEC w145[2] = {                                           \
      (T_VEC)(w112[0] + (-kWeight3 * w136[0] - kWeight4 * w136[1])),  \
      (T_VEC)(w112[1] + (-kWeight3 * w136[1] + kWeight4 * w136[0]))   \
    };                                                                \
    const T_VEC w146[2] = {                                           \
      (T_VEC)(w114[0] + (kWeight2 * w138[0] + kWeight2 * w138[1])),   \
      (T_VEC)(w114[1] + (kWeight2 * w138[1] - kWeight2 * w138[0]))    \
    };                                                                \
    const T_VEC w147[2] = {                                           \
      (T_VEC)(w114[0] + (-kWeight2 * w138[0] - kWeight2 * w138[1])),  \
      (T_VEC)(w114[1] + (-kWeight2 * w138[1] + kWeight2 * w138[0]))   \
    };                                                                \
    const T_VEC w148[2] = {                                           \
      (T_VEC)(w116[0] + (kWeight4 * w140[0] + kWeight3 * w140[1])),   \
      (T_VEC)(w116[1] + (kWeight4 * w140[1] - kWeight3 * w140[0]))    \
    };                                                                \
    const T_VEC w149[2] = {                                           \
      (T_VEC)(w116[0] + (-kWeight4 * w140[0] - kWeight3 * w140[1])),  \
      (T_VEC)(w116[1] + (-kWeight4 * w140[1] + kWeight3 * w140[0]))   \
    };                                                                \
    const T_VEC w150[2] = { (T_VEC)(w111[0] + w135[1]),               \
                            (T_VEC)(w111[1] - w135[0]) };             \
    const T_VEC w151[2] = { (T_VEC)(w111[0] - w135[1]),               \
                            (T_VEC)(w111[1] + w135[0]) };             \
    const T_VEC w152[2] = {                                           \
      (T_VEC)(w113[0] + (-kWeight4 * w137[0] + kWeight3 * w137[1])),  \
      (T_VEC)(w113[1] + (-kWeight4 * w137[1] - kWeight3 * w137[0]))   \
    };                                                                \
    const T_VEC w153[2] = {                                           \
      (T_VEC)(w113[0] + (kWeight4 * w137[0] - kWeight3 * w137[1])),   \
      (T_VEC)(w113[1] + (kWeight4 * w137[1] + kWeight3 * w137[0]))    \
    };                                                                \
    const T_VEC w154[2] = {                                           \
      (T_VEC)(w115[0] + (-kWeight2 * w139[0] + kWeight2 * w139[1])),  \
      (T_VEC)(w115[1] + (-kWeight2 * w139[1] - kWeight2 * w139[0]))   \
    };                                                                \
    const T_VEC w155[2] = {                                           \
      (T_VEC)(w115[0] + (kWeight2 * w139[0] - kWeight2 * w139[1])),   \
      (T_VEC)(w115[1] + (kWeight2 * w139[1] + kWeight2 * w139[0]))    \
    };                                                                \
    const T_VEC w156[2] = {                                           \
      (T_VEC)(w117[0] + (-kWeight3 * w141[0] + kWeight4 * w141[1])),  \
      (T_VEC)(w117[1] + (-kWeight3 * w141[1] - kWeight4 * w141[0]))   \
    };                                                                \
    const T_VEC w157[2] = {                                           \
      (T_VEC)(w117[0] + (kWeight3 * w141[0] - kWeight4 * w141[1])),   \
      (T_VEC)(w117[1] + (kWeight3 * w141[1] + kWeight4 * w141[0]))    \
    };                                                                \
    store(output + 0 * stride, w78[0] + w142[0]);                     \
    store(output + 1 * stride,                                        \
          w80[0] + (kWeight5 * w144[0] + kWeight6 * w144[1]));        \
    store(output + 2 * stride,                                        \
          w82[0] + (kWeight3 * w146[0] + kWeight4 * w146[1]));        \
    store(output + 3 * stride,                                        \
          w84[0] + (kWeight7 * w148[0] + kWeight8 * w148[1]));        \
    store(output + 4 * stride,                                        \
          w86[0] + (kWeight2 * w150[0] + kWeight2 * w150[1]));        \
    store(output + 5 * stride,                                        \
          w88[0] + (kWeight8 * w152[0] + kWeight7 * w152[1]));        \
    store(output + 6 * stride,                                        \
          w90[0] + (kWeight4 * w154[0] + kWeight3 * w154[1]));        \
    store(output + 7 * stride,                                        \
          w92[0] + (kWeight6 * w156[0] + kWeight5 * w156[1]));        \
    store(output + 8 * stride, w79[0] + w143[1]);                     \
    store(output + 9 * stride,                                        \
          w81[0] + (-kWeight6 * w145[0] + kWeight5 * w145[1]));       \
    store(output + 10 * stride,                                       \
          w83[0] + (-kWeight4 * w147[0] + kWeight3 * w147[1]));       \
    store(output + 11 * stride,                                       \
          w85[0] + (-kWeight8 * w149[0] + kWeight7 * w149[1]));       \
    store(output + 12 * stride,                                       \
          w87[0] + (-kWeight2 * w151[0] + kWeight2 * w151[1]));       \
    store(output + 13 * stride,                                       \
          w89[0] + (-kWeight7 * w153[0] + kWeight8 * w153[1]));       \
    store(output + 14 * stride,                                       \
          w91[0] + (-kWeight3 * w155[0] + kWeight4 * w155[1]));       \
    store(output + 15 * stride,                                       \
          w93[0] + (-kWeight5 * w157[0] + kWeight6 * w157[1]));       \
    store(output + 16 * stride, w78[0] - w142[0]);                    \
    store(output + 17 * stride,                                       \
          w80[0] + (-kWeight5 * w144[0] - kWeight6 * w144[1]));       \
    store(output + 18 * stride,                                       \
          w82[0] + (-kWeight3 * w146[0] - kWeight4 * w146[1]));       \
    store(output + 19 * stride,                                       \
          w84[0] + (-kWeight7 * w148[0] - kWeight8 * w148[1]));       \
    store(output + 20 * stride,                                       \
          w86[0] + (-kWeight2 * w150[0] - kWeight2 * w150[1]));       \
    store(output + 21 * stride,                                       \
          w88[0] + (-kWeight8 * w152[0] - kWeight7 * w152[1]));       \
    store(output + 22 * stride,                                       \
          w90[0] + (-kWeight4 * w154[0] - kWeight3 * w154[1]));       \
    store(output + 23 * stride,                                       \
          w92[0] + (-kWeight6 * w156[0] - kWeight5 * w156[1]));       \
    store(output + 24 * stride, w79[0] - w143[1]);                    \
    store(output + 25 * stride,                                       \
          w81[0] + (kWeight6 * w145[0] - kWeight5 * w145[1]));        \
    store(output + 26 * stride,                                       \
          w83[0] + (kWeight4 * w147[0] - kWeight3 * w147[1]));        \
    store(output + 27 * stride,                                       \
          w85[0] + (kWeight8 * w149[0] - kWeight7 * w149[1]));        \
    store(output + 28 * stride,                                       \
          w87[0] + (kWeight2 * w151[0] - kWeight2 * w151[1]));        \
    store(output + 29 * stride,                                       \
          w89[0] + (kWeight7 * w153[0] - kWeight8 * w153[1]));        \
    store(output + 30 * stride,                                       \
          w91[0] + (kWeight3 * w155[0] - kWeight4 * w155[1]));        \
    store(output + 31 * stride,                                       \
          w93[0] + (kWeight5 * w157[0] - kWeight6 * w157[1]));        \
  }

#endif  // AOM_DSP_FFT_COMMON_H_
