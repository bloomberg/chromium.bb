sub aom_dsp_forward_decls() {
print <<EOF
/*
 * DSP
 */

#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"

EOF
}
forward_decls qw/aom_dsp_forward_decls/;

# x86inc.asm had specific constraints. break it out so it's easy to disable.
# zero all the variables to avoid tricky else conditions.
$mmx_x86inc = $sse_x86inc = $sse2_x86inc = $ssse3_x86inc = $avx_x86inc =
  $avx2_x86inc = '';
$mmx_x86_64_x86inc = $sse_x86_64_x86inc = $sse2_x86_64_x86inc =
  $ssse3_x86_64_x86inc = $avx_x86_64_x86inc = $avx2_x86_64_x86inc = '';
if (aom_config("CONFIG_USE_X86INC") eq "yes") {
  $mmx_x86inc = 'mmx';
  $sse_x86inc = 'sse';
  $sse2_x86inc = 'sse2';
  $ssse3_x86inc = 'ssse3';
  $avx_x86inc = 'avx';
  $avx2_x86inc = 'avx2';
  if ($opts{arch} eq "x86_64") {
    $mmx_x86_64_x86inc = 'mmx';
    $sse_x86_64_x86inc = 'sse';
    $sse2_x86_64_x86inc = 'sse2';
    $ssse3_x86_64_x86inc = 'ssse3';
    $avx_x86_64_x86inc = 'avx';
    $avx2_x86_64_x86inc = 'avx2';
  }
}

# optimizations which depend on multiple features
$avx2_ssse3 = '';
if ((aom_config("HAVE_AVX2") eq "yes") && (aom_config("HAVE_SSSE3") eq "yes")) {
  $avx2_ssse3 = 'avx2';
}

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
# Intra prediction
#

add_proto qw/void aom_d207_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d207_predictor_4x4/, "$ssse3_x86inc";

add_proto qw/void aom_d207e_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d207e_predictor_4x4/;

add_proto qw/void aom_d45_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d45_predictor_4x4 neon/, "$ssse3_x86inc";

add_proto qw/void aom_d45e_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d45e_predictor_4x4/;

add_proto qw/void aom_d63_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d63_predictor_4x4/, "$ssse3_x86inc";

add_proto qw/void aom_d63e_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d63e_predictor_4x4/;

add_proto qw/void aom_d63f_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d63f_predictor_4x4/;

add_proto qw/void aom_h_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_h_predictor_4x4 neon dspr2 msa/, "$sse2_x86inc";

add_proto qw/void aom_he_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_he_predictor_4x4/;

add_proto qw/void aom_d117_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d117_predictor_4x4/;

add_proto qw/void aom_d135_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d135_predictor_4x4 neon/;

add_proto qw/void aom_d153_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d153_predictor_4x4/, "$ssse3_x86inc";

add_proto qw/void aom_v_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_v_predictor_4x4 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_ve_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_ve_predictor_4x4/;

add_proto qw/void aom_tm_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_tm_predictor_4x4 neon dspr2 msa/, "$sse2_x86inc";

add_proto qw/void aom_dc_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_predictor_4x4 dspr2 msa neon/, "$sse2_x86inc";

add_proto qw/void aom_dc_top_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_top_predictor_4x4 msa neon/, "$sse2_x86inc";

add_proto qw/void aom_dc_left_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_left_predictor_4x4 msa neon/, "$sse2_x86inc";

add_proto qw/void aom_dc_128_predictor_4x4/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_128_predictor_4x4 msa neon/, "$sse2_x86inc";

add_proto qw/void aom_d207_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d207_predictor_8x8/, "$ssse3_x86inc";

add_proto qw/void aom_d207e_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d207e_predictor_8x8/;

add_proto qw/void aom_d45_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d45_predictor_8x8 neon/, "$ssse3_x86inc";

add_proto qw/void aom_d45e_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d45e_predictor_8x8/;

add_proto qw/void aom_d63_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d63_predictor_8x8/, "$ssse3_x86inc";

add_proto qw/void aom_d63e_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d63e_predictor_8x8/;

add_proto qw/void aom_h_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_h_predictor_8x8 neon dspr2 msa/, "$sse2_x86inc";

add_proto qw/void aom_d117_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d117_predictor_8x8/;

add_proto qw/void aom_d135_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d135_predictor_8x8/;

add_proto qw/void aom_d153_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d153_predictor_8x8/, "$ssse3_x86inc";

add_proto qw/void aom_v_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_v_predictor_8x8 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_tm_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_tm_predictor_8x8 neon dspr2 msa/, "$sse2_x86inc";

add_proto qw/void aom_dc_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_predictor_8x8 dspr2 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_dc_top_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_top_predictor_8x8 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_dc_left_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_left_predictor_8x8 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_dc_128_predictor_8x8/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_128_predictor_8x8 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_d207_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d207_predictor_16x16/, "$ssse3_x86inc";

add_proto qw/void aom_d207e_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d207e_predictor_16x16/;

add_proto qw/void aom_d45_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d45_predictor_16x16 neon/, "$ssse3_x86inc";

add_proto qw/void aom_d45e_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d45e_predictor_16x16/;

add_proto qw/void aom_d63_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d63_predictor_16x16/, "$ssse3_x86inc";

add_proto qw/void aom_d63e_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d63e_predictor_16x16/;

add_proto qw/void aom_h_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_h_predictor_16x16 neon dspr2 msa/, "$sse2_x86inc";

add_proto qw/void aom_d117_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d117_predictor_16x16/;

add_proto qw/void aom_d135_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d135_predictor_16x16/;

add_proto qw/void aom_d153_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d153_predictor_16x16/, "$ssse3_x86inc";

add_proto qw/void aom_v_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_v_predictor_16x16 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_tm_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_tm_predictor_16x16 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_dc_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_predictor_16x16 dspr2 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_dc_top_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_top_predictor_16x16 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_dc_left_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_left_predictor_16x16 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_dc_128_predictor_16x16/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_128_predictor_16x16 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_d207_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d207_predictor_32x32/, "$ssse3_x86inc";

add_proto qw/void aom_d207e_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d207e_predictor_32x32/;

add_proto qw/void aom_d45_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d45_predictor_32x32/, "$ssse3_x86inc";

add_proto qw/void aom_d45e_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d45e_predictor_32x32/;

add_proto qw/void aom_d63_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d63_predictor_32x32/, "$ssse3_x86inc";

add_proto qw/void aom_d63e_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d63e_predictor_32x32/;

add_proto qw/void aom_h_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_h_predictor_32x32 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_d117_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d117_predictor_32x32/;

add_proto qw/void aom_d135_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d135_predictor_32x32/;

add_proto qw/void aom_d153_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_d153_predictor_32x32/, "$ssse3_x86inc";

add_proto qw/void aom_v_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_v_predictor_32x32 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_tm_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_tm_predictor_32x32 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_dc_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_predictor_32x32 msa neon/, "$sse2_x86inc";

add_proto qw/void aom_dc_top_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_top_predictor_32x32 msa neon/, "$sse2_x86inc";

add_proto qw/void aom_dc_left_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_left_predictor_32x32 msa neon/, "$sse2_x86inc";

add_proto qw/void aom_dc_128_predictor_32x32/, "uint8_t *dst, ptrdiff_t y_stride, const uint8_t *above, const uint8_t *left";
specialize qw/aom_dc_128_predictor_32x32 msa neon/, "$sse2_x86inc";

# High bitdepth functions
if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void aom_highbd_d207_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d207_predictor_4x4/;

  add_proto qw/void aom_highbd_d207e_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d207e_predictor_4x4/;

  add_proto qw/void aom_highbd_d45_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d45_predictor_4x4/;

  add_proto qw/void aom_highbd_d45e_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d45e_predictor_4x4/;

  add_proto qw/void aom_highbd_d63_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d63_predictor_4x4/;

  add_proto qw/void aom_highbd_d63e_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d63e_predictor_4x4/;

  add_proto qw/void aom_highbd_h_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_h_predictor_4x4/;

  add_proto qw/void aom_highbd_d117_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d117_predictor_4x4/;

  add_proto qw/void aom_highbd_d135_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d135_predictor_4x4/;

  add_proto qw/void aom_highbd_d153_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d153_predictor_4x4/;

  add_proto qw/void aom_highbd_v_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_v_predictor_4x4/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_tm_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_tm_predictor_4x4/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_dc_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_predictor_4x4/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_dc_top_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_top_predictor_4x4/;

  add_proto qw/void aom_highbd_dc_left_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_left_predictor_4x4/;

  add_proto qw/void aom_highbd_dc_128_predictor_4x4/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_128_predictor_4x4/;

  add_proto qw/void aom_highbd_d207_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d207_predictor_8x8/;

  add_proto qw/void aom_highbd_d207e_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d207e_predictor_8x8/;

  add_proto qw/void aom_highbd_d45_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d45_predictor_8x8/;

  add_proto qw/void aom_highbd_d45e_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d45e_predictor_8x8/;

  add_proto qw/void aom_highbd_d63_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d63_predictor_8x8/;

  add_proto qw/void aom_highbd_d63e_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d63e_predictor_8x8/;

  add_proto qw/void aom_highbd_h_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_h_predictor_8x8/;

  add_proto qw/void aom_highbd_d117_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d117_predictor_8x8/;

  add_proto qw/void aom_highbd_d135_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d135_predictor_8x8/;

  add_proto qw/void aom_highbd_d153_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d153_predictor_8x8/;

  add_proto qw/void aom_highbd_v_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_v_predictor_8x8/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_tm_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_tm_predictor_8x8/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_dc_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_predictor_8x8/, "$sse2_x86inc";;

  add_proto qw/void aom_highbd_dc_top_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_top_predictor_8x8/;

  add_proto qw/void aom_highbd_dc_left_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_left_predictor_8x8/;

  add_proto qw/void aom_highbd_dc_128_predictor_8x8/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_128_predictor_8x8/;

  add_proto qw/void aom_highbd_d207_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d207_predictor_16x16/;

  add_proto qw/void aom_highbd_d207e_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d207e_predictor_16x16/;

  add_proto qw/void aom_highbd_d45_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d45_predictor_16x16/;

  add_proto qw/void aom_highbd_d45e_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d45e_predictor_16x16/;

  add_proto qw/void aom_highbd_d63_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d63_predictor_16x16/;

  add_proto qw/void aom_highbd_d63e_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d63e_predictor_16x16/;

  add_proto qw/void aom_highbd_h_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_h_predictor_16x16/;

  add_proto qw/void aom_highbd_d117_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d117_predictor_16x16/;

  add_proto qw/void aom_highbd_d135_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d135_predictor_16x16/;

  add_proto qw/void aom_highbd_d153_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d153_predictor_16x16/;

  add_proto qw/void aom_highbd_v_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_v_predictor_16x16/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_tm_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_tm_predictor_16x16/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_dc_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_predictor_16x16/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_dc_top_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_top_predictor_16x16/;

  add_proto qw/void aom_highbd_dc_left_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_left_predictor_16x16/;

  add_proto qw/void aom_highbd_dc_128_predictor_16x16/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_128_predictor_16x16/;

  add_proto qw/void aom_highbd_d207_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d207_predictor_32x32/;

  add_proto qw/void aom_highbd_d207e_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d207e_predictor_32x32/;

  add_proto qw/void aom_highbd_d45_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d45_predictor_32x32/;

  add_proto qw/void aom_highbd_d45e_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d45e_predictor_32x32/;

  add_proto qw/void aom_highbd_d63_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d63_predictor_32x32/;

  add_proto qw/void aom_highbd_d63e_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d63e_predictor_32x32/;

  add_proto qw/void aom_highbd_h_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_h_predictor_32x32/;

  add_proto qw/void aom_highbd_d117_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d117_predictor_32x32/;

  add_proto qw/void aom_highbd_d135_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d135_predictor_32x32/;

  add_proto qw/void aom_highbd_d153_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_d153_predictor_32x32/;

  add_proto qw/void aom_highbd_v_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_v_predictor_32x32/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_tm_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_tm_predictor_32x32/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_dc_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_predictor_32x32/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_dc_top_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_top_predictor_32x32/;

  add_proto qw/void aom_highbd_dc_left_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_left_predictor_32x32/;

  add_proto qw/void aom_highbd_dc_128_predictor_32x32/, "uint16_t *dst, ptrdiff_t y_stride, const uint16_t *above, const uint16_t *left, int bd";
  specialize qw/aom_highbd_dc_128_predictor_32x32/;
}  # CONFIG_AOM_HIGHBITDEPTH

#
# Sub Pixel Filters
#
add_proto qw/void aom_convolve_copy/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/aom_convolve_copy neon dspr2 msa/, "$sse2_x86inc";

add_proto qw/void aom_convolve_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/aom_convolve_avg neon dspr2 msa/, "$sse2_x86inc";

add_proto qw/void aom_convolve8/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/aom_convolve8 sse2 ssse3 neon dspr2 msa/, "$avx2_ssse3";

add_proto qw/void aom_convolve8_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/aom_convolve8_horiz sse2 ssse3 neon dspr2 msa/, "$avx2_ssse3";

add_proto qw/void aom_convolve8_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/aom_convolve8_vert sse2 ssse3 neon dspr2 msa/, "$avx2_ssse3";

add_proto qw/void aom_convolve8_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/aom_convolve8_avg sse2 ssse3 neon dspr2 msa/;

add_proto qw/void aom_convolve8_avg_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/aom_convolve8_avg_horiz sse2 ssse3 neon dspr2 msa/;

add_proto qw/void aom_convolve8_avg_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/aom_convolve8_avg_vert sse2 ssse3 neon dspr2 msa/;

add_proto qw/void aom_scaled_2d/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/aom_scaled_2d ssse3/;

add_proto qw/void aom_scaled_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/aom_scaled_horiz/;

add_proto qw/void aom_scaled_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/aom_scaled_vert/;

add_proto qw/void aom_scaled_avg_2d/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/aom_scaled_avg_2d/;

add_proto qw/void aom_scaled_avg_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/aom_scaled_avg_horiz/;

add_proto qw/void aom_scaled_avg_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h";
specialize qw/aom_scaled_avg_vert/;

if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
  #
  # Sub Pixel Filters
  #
  add_proto qw/void aom_highbd_convolve_copy/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/aom_highbd_convolve_copy/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_convolve_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/aom_highbd_convolve_avg/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_convolve8/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/aom_highbd_convolve8/, "$sse2_x86_64";

  add_proto qw/void aom_highbd_convolve8_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/aom_highbd_convolve8_horiz/, "$sse2_x86_64";

  add_proto qw/void aom_highbd_convolve8_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/aom_highbd_convolve8_vert/, "$sse2_x86_64";

  add_proto qw/void aom_highbd_convolve8_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/aom_highbd_convolve8_avg/, "$sse2_x86_64";

  add_proto qw/void aom_highbd_convolve8_avg_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/aom_highbd_convolve8_avg_horiz/, "$sse2_x86_64";

  add_proto qw/void aom_highbd_convolve8_avg_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/aom_highbd_convolve8_avg_vert/, "$sse2_x86_64";
}  # CONFIG_AOM_HIGHBITDEPTH

#
# Loopfilter
#
add_proto qw/void aom_lpf_vertical_16/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_vertical_16 sse2 neon_asm dspr2 msa/;
$aom_lpf_vertical_16_neon_asm=aom_lpf_vertical_16_neon;

add_proto qw/void aom_lpf_vertical_16_dual/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh";
specialize qw/aom_lpf_vertical_16_dual sse2 neon_asm dspr2 msa/;
$aom_lpf_vertical_16_dual_neon_asm=aom_lpf_vertical_16_dual_neon;

add_proto qw/void aom_lpf_vertical_8/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/aom_lpf_vertical_8 sse2 neon dspr2 msa/;

add_proto qw/void aom_lpf_vertical_8_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_vertical_8_dual sse2 neon_asm dspr2 msa/;
$aom_lpf_vertical_8_dual_neon_asm=aom_lpf_vertical_8_dual_neon;

add_proto qw/void aom_lpf_vertical_4/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/aom_lpf_vertical_4 mmx neon dspr2 msa/;

add_proto qw/void aom_lpf_vertical_4_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_vertical_4_dual sse2 neon dspr2 msa/;

add_proto qw/void aom_lpf_horizontal_16/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/aom_lpf_horizontal_16 sse2 avx2 neon_asm dspr2 msa/;
$aom_lpf_horizontal_16_neon_asm=aom_lpf_horizontal_16_neon;

add_proto qw/void aom_lpf_horizontal_8/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/aom_lpf_horizontal_8 sse2 neon dspr2 msa/;

add_proto qw/void aom_lpf_horizontal_8_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_horizontal_8_dual sse2 neon_asm dspr2 msa/;
$aom_lpf_horizontal_8_dual_neon_asm=aom_lpf_horizontal_8_dual_neon;

add_proto qw/void aom_lpf_horizontal_4/, "uint8_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count";
specialize qw/aom_lpf_horizontal_4 mmx neon dspr2 msa/;

add_proto qw/void aom_lpf_horizontal_4_dual/, "uint8_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1";
specialize qw/aom_lpf_horizontal_4_dual sse2 neon dspr2 msa/;

if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void aom_highbd_lpf_vertical_16/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
  specialize qw/aom_highbd_lpf_vertical_16 sse2/;

  add_proto qw/void aom_highbd_lpf_vertical_16_dual/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int bd";
  specialize qw/aom_highbd_lpf_vertical_16_dual sse2/;

  add_proto qw/void aom_highbd_lpf_vertical_8/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count, int bd";
  specialize qw/aom_highbd_lpf_vertical_8 sse2/;

  add_proto qw/void aom_highbd_lpf_vertical_8_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/aom_highbd_lpf_vertical_8_dual sse2/;

  add_proto qw/void aom_highbd_lpf_vertical_4/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count, int bd";
  specialize qw/aom_highbd_lpf_vertical_4 sse2/;

  add_proto qw/void aom_highbd_lpf_vertical_4_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/aom_highbd_lpf_vertical_4_dual sse2/;

  add_proto qw/void aom_highbd_lpf_horizontal_16/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count, int bd";
  specialize qw/aom_highbd_lpf_horizontal_16 sse2/;

  add_proto qw/void aom_highbd_lpf_horizontal_8/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count, int bd";
  specialize qw/aom_highbd_lpf_horizontal_8 sse2/;

  add_proto qw/void aom_highbd_lpf_horizontal_8_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/aom_highbd_lpf_horizontal_8_dual sse2/;

  add_proto qw/void aom_highbd_lpf_horizontal_4/, "uint16_t *s, int pitch, const uint8_t *blimit, const uint8_t *limit, const uint8_t *thresh, int count, int bd";
  specialize qw/aom_highbd_lpf_horizontal_4 sse2/;

  add_proto qw/void aom_highbd_lpf_horizontal_4_dual/, "uint16_t *s, int pitch, const uint8_t *blimit0, const uint8_t *limit0, const uint8_t *thresh0, const uint8_t *blimit1, const uint8_t *limit1, const uint8_t *thresh1, int bd";
  specialize qw/aom_highbd_lpf_horizontal_4_dual sse2/;
}  # CONFIG_AOM_HIGHBITDEPTH

#
# Encoder functions.
#

#
# Forward transform
#
if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {
if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void aom_fdct4x4/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct4x4 sse2/;

  add_proto qw/void aom_fdct4x4_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct4x4_1 sse2/;

  add_proto qw/void aom_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct8x8 sse2/;

  add_proto qw/void aom_fdct8x8_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct8x8_1 sse2/;

  add_proto qw/void aom_fdct16x16/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct16x16 sse2/;

  add_proto qw/void aom_fdct16x16_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct16x16_1 sse2/;

  add_proto qw/void aom_fdct32x32/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct32x32 sse2/;

  add_proto qw/void aom_fdct32x32_rd/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct32x32_rd sse2/;

  add_proto qw/void aom_fdct32x32_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct32x32_1 sse2/;

  add_proto qw/void aom_highbd_fdct4x4/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_highbd_fdct4x4 sse2/;

  add_proto qw/void aom_highbd_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_highbd_fdct8x8 sse2/;

  add_proto qw/void aom_highbd_fdct8x8_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_highbd_fdct8x8_1/;

  add_proto qw/void aom_highbd_fdct16x16/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_highbd_fdct16x16 sse2/;

  add_proto qw/void aom_highbd_fdct16x16_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_highbd_fdct16x16_1/;

  add_proto qw/void aom_highbd_fdct32x32/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_highbd_fdct32x32 sse2/;

  add_proto qw/void aom_highbd_fdct32x32_rd/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_highbd_fdct32x32_rd sse2/;

  add_proto qw/void aom_highbd_fdct32x32_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_highbd_fdct32x32_1/;
} else {
  add_proto qw/void aom_fdct4x4/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct4x4 sse2 msa/;

  add_proto qw/void aom_fdct4x4_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct4x4_1 sse2/;

  add_proto qw/void aom_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct8x8 sse2 neon msa/, "$ssse3_x86_64_x86inc";

  add_proto qw/void aom_fdct8x8_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct8x8_1 sse2 neon msa/;

  add_proto qw/void aom_fdct16x16/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct16x16 sse2 msa/;

  add_proto qw/void aom_fdct16x16_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct16x16_1 sse2 msa/;

  add_proto qw/void aom_fdct32x32/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct32x32 sse2 avx2 msa/;

  add_proto qw/void aom_fdct32x32_rd/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct32x32_rd sse2 avx2 msa/;

  add_proto qw/void aom_fdct32x32_1/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/aom_fdct32x32_1 sse2 msa/;
}  # CONFIG_AOM_HIGHBITDEPTH
}  # CONFIG_AV1_ENCODER

#
# Inverse transform
if (aom_config("CONFIG_AV1") eq "yes") {
if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
  # Note as optimized versions of these functions are added we need to add a check to ensure
  # that when CONFIG_EMULATE_HARDWARE is on, it defaults to the C versions only.
  add_proto qw/void aom_iwht4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/aom_iwht4x4_1_add/;

  add_proto qw/void aom_iwht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/aom_iwht4x4_16_add/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_idct4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/aom_highbd_idct4x4_1_add/;

  add_proto qw/void aom_highbd_idct8x8_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/aom_highbd_idct8x8_1_add/;

  add_proto qw/void aom_highbd_idct16x16_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/aom_highbd_idct16x16_1_add/;

  add_proto qw/void aom_highbd_idct32x32_1024_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/aom_highbd_idct32x32_1024_add/;

  add_proto qw/void aom_highbd_idct32x32_34_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/aom_highbd_idct32x32_34_add/;

  add_proto qw/void aom_highbd_idct32x32_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/aom_highbd_idct32x32_1_add/;

  add_proto qw/void aom_highbd_iwht4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/aom_highbd_iwht4x4_1_add/;

  add_proto qw/void aom_highbd_iwht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/aom_highbd_iwht4x4_16_add/;

  # Force C versions if CONFIG_EMULATE_HARDWARE is 1
  if (aom_config("CONFIG_EMULATE_HARDWARE") eq "yes") {
    add_proto qw/void aom_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct4x4_16_add/;

    add_proto qw/void aom_idct4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct4x4_1_add/;

    add_proto qw/void aom_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct8x8_64_add/;

    add_proto qw/void aom_idct8x8_12_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct8x8_12_add/;

    add_proto qw/void aom_idct8x8_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct8x8_1_add/;

    add_proto qw/void aom_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct16x16_256_add/;

    add_proto qw/void aom_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct16x16_10_add/;

    add_proto qw/void aom_idct16x16_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct16x16_1_add/;

    add_proto qw/void aom_idct32x32_1024_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_1024_add/;

    add_proto qw/void aom_idct32x32_135_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_135_add/;

    add_proto qw/void aom_idct32x32_34_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_34_add/;

    add_proto qw/void aom_idct32x32_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_1_add/;

    add_proto qw/void aom_highbd_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/aom_highbd_idct4x4_16_add/;

    add_proto qw/void aom_highbd_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/aom_highbd_idct8x8_64_add/;

    add_proto qw/void aom_highbd_idct8x8_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/aom_highbd_idct8x8_10_add/;

    add_proto qw/void aom_highbd_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/aom_highbd_idct16x16_256_add/;

    add_proto qw/void aom_highbd_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/aom_highbd_idct16x16_10_add/;
  } else {
    add_proto qw/void aom_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct4x4_16_add sse2/;

    add_proto qw/void aom_idct4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct4x4_1_add sse2/;

    add_proto qw/void aom_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct8x8_64_add sse2/, "$ssse3_x86_64_x86inc";

    add_proto qw/void aom_idct8x8_12_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct8x8_12_add sse2/, "$ssse3_x86_64_x86inc";

    add_proto qw/void aom_idct8x8_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct8x8_1_add sse2/;

    add_proto qw/void aom_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct16x16_256_add sse2/;

    add_proto qw/void aom_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct16x16_10_add sse2/;

    add_proto qw/void aom_idct16x16_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct16x16_1_add sse2/;

    add_proto qw/void aom_idct32x32_1024_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_1024_add sse2/, "$ssse3_x86_64_x86inc";

    add_proto qw/void aom_idct32x32_135_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_135_add sse2/, "$ssse3_x86_64_x86inc";
    # Need to add 135 eob idct32x32 implementations.
    $aom_idct32x32_135_add_sse2=aom_idct32x32_1024_add_sse2;

    add_proto qw/void aom_idct32x32_34_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_34_add sse2/, "$ssse3_x86_64_x86inc";

    add_proto qw/void aom_idct32x32_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_1_add sse2/;

    add_proto qw/void aom_highbd_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/aom_highbd_idct4x4_16_add sse2/;

    add_proto qw/void aom_highbd_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/aom_highbd_idct8x8_64_add sse2/;

    add_proto qw/void aom_highbd_idct8x8_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/aom_highbd_idct8x8_10_add sse2/;

    add_proto qw/void aom_highbd_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/aom_highbd_idct16x16_256_add sse2/;

    add_proto qw/void aom_highbd_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/aom_highbd_idct16x16_10_add sse2/;
  }  # CONFIG_EMULATE_HARDWARE
} else {
  # Force C versions if CONFIG_EMULATE_HARDWARE is 1
  if (aom_config("CONFIG_EMULATE_HARDWARE") eq "yes") {
    add_proto qw/void aom_idct4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct4x4_1_add/;

    add_proto qw/void aom_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct4x4_16_add/;

    add_proto qw/void aom_idct8x8_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct8x8_1_add/;

    add_proto qw/void aom_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct8x8_64_add/;

    add_proto qw/void aom_idct8x8_12_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct8x8_12_add/;

    add_proto qw/void aom_idct16x16_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct16x16_1_add/;

    add_proto qw/void aom_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct16x16_256_add/;

    add_proto qw/void aom_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct16x16_10_add/;

    add_proto qw/void aom_idct32x32_1024_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_1024_add/;

    add_proto qw/void aom_idct32x32_135_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_135_add/;

    add_proto qw/void aom_idct32x32_34_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_34_add/;

    add_proto qw/void aom_idct32x32_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_1_add/;

    add_proto qw/void aom_iwht4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_iwht4x4_1_add/;

    add_proto qw/void aom_iwht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_iwht4x4_16_add/;
  } else {
    add_proto qw/void aom_idct4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct4x4_1_add sse2 neon dspr2 msa/;

    add_proto qw/void aom_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct4x4_16_add sse2 neon dspr2 msa/;

    add_proto qw/void aom_idct8x8_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct8x8_1_add sse2 neon dspr2 msa/;

    add_proto qw/void aom_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct8x8_64_add sse2 neon dspr2 msa/, "$ssse3_x86_64_x86inc";

    add_proto qw/void aom_idct8x8_12_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct8x8_12_add sse2 neon dspr2 msa/, "$ssse3_x86_64_x86inc";

    add_proto qw/void aom_idct16x16_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct16x16_1_add sse2 neon dspr2 msa/;

    add_proto qw/void aom_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct16x16_256_add sse2 neon dspr2 msa/;

    add_proto qw/void aom_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct16x16_10_add sse2 neon dspr2 msa/;

    add_proto qw/void aom_idct32x32_1024_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_1024_add sse2 neon dspr2 msa/, "$ssse3_x86_64_x86inc";

    add_proto qw/void aom_idct32x32_135_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_135_add sse2 neon dspr2 msa/, "$ssse3_x86_64_x86inc";
    # Need to add 135 eob idct32x32 implementations.
    $aom_idct32x32_135_add_sse2=aom_idct32x32_1024_add_sse2;
    $aom_idct32x32_135_add_neon=aom_idct32x32_1024_add_neon;
    $aom_idct32x32_135_add_dspr2=aom_idct32x32_1024_add_dspr2;
    $aom_idct32x32_135_add_msa=aom_idct32x32_1024_add_msa;

    add_proto qw/void aom_idct32x32_34_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_34_add sse2 neon_asm dspr2 msa/, "$ssse3_x86_64_x86inc";
    # Need to add 34 eob idct32x32 neon implementation.
    $aom_idct32x32_34_add_neon_asm=aom_idct32x32_1024_add_neon;

    add_proto qw/void aom_idct32x32_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_idct32x32_1_add sse2 neon dspr2 msa/;

    add_proto qw/void aom_iwht4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_iwht4x4_1_add msa/;

    add_proto qw/void aom_iwht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/aom_iwht4x4_16_add msa/, "$sse2_x86inc";
  }  # CONFIG_EMULATE_HARDWARE
}  # CONFIG_AOM_HIGHBITDEPTH
}  # CONFIG_AV1

#
# Quantization
#
if (aom_config("CONFIG_AOM_QM") eq "yes") {
  if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {
    add_proto qw/void aom_quantize_b/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t * iqm_ptr";

    add_proto qw/void aom_quantize_b_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t * iqm_ptr";

    if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
      add_proto qw/void aom_highbd_quantize_b/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t * iqm_ptr";

      add_proto qw/void aom_highbd_quantize_b_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t * iqm_ptr";
    }  # CONFIG_AOM_HIGHBITDEPTH
  }  # CONFIG_AV1_ENCODER
} else {
  if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {
    add_proto qw/void aom_quantize_b/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/aom_quantize_b sse2/, "$ssse3_x86_64_x86inc", "$avx_x86_64_x86inc";

    add_proto qw/void aom_quantize_b_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/aom_quantize_b_32x32/, "$ssse3_x86_64_x86inc", "$avx_x86_64_x86inc";

    if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
      add_proto qw/void aom_highbd_quantize_b/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
      specialize qw/aom_highbd_quantize_b sse2/;

      add_proto qw/void aom_highbd_quantize_b_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
      specialize qw/aom_highbd_quantize_b_32x32 sse2/;
    }  # CONFIG_AOM_HIGHBITDEPTH
  }  # CONFIG_AV1_ENCODER
} # CONFIG_AOM_QM

if (aom_config("CONFIG_ENCODERS") eq "yes") {
#
# Block subtraction
#
add_proto qw/void aom_subtract_block/, "int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride";
specialize qw/aom_subtract_block neon msa/, "$sse2_x86inc";

#
# Single block SAD
#
add_proto qw/unsigned int aom_sad64x64/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/aom_sad64x64 avx2 neon msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad64x32/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/aom_sad64x32 avx2 msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad32x64/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/aom_sad32x64 avx2 msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad32x32/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/aom_sad32x32 avx2 neon msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad32x16/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/aom_sad32x16 avx2 msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad16x32/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/aom_sad16x32 msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad16x16/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/aom_sad16x16 media neon msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad16x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/aom_sad16x8 neon msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad8x16/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/aom_sad8x16 neon msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/aom_sad8x8 neon msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad8x4/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/aom_sad8x4 msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad4x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/aom_sad4x8 msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad4x4/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
specialize qw/aom_sad4x4 neon msa/, "$sse2_x86inc";

#
# Avg
#
if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {
  add_proto qw/unsigned int aom_avg_8x8/, "const uint8_t *, int p";
  specialize qw/aom_avg_8x8 sse2 neon msa/;

  add_proto qw/unsigned int aom_avg_4x4/, "const uint8_t *, int p";
  specialize qw/aom_avg_4x4 sse2 neon msa/;

  add_proto qw/void aom_minmax_8x8/, "const uint8_t *s, int p, const uint8_t *d, int dp, int *min, int *max";
  specialize qw/aom_minmax_8x8 sse2/;

  add_proto qw/void aom_hadamard_8x8/, "int16_t const *src_diff, int src_stride, int16_t *coeff";
  specialize qw/aom_hadamard_8x8 sse2/, "$ssse3_x86_64_x86inc";

  add_proto qw/void aom_hadamard_16x16/, "int16_t const *src_diff, int src_stride, int16_t *coeff";
  specialize qw/aom_hadamard_16x16 sse2/;

  add_proto qw/int aom_satd/, "const int16_t *coeff, int length";
  specialize qw/aom_satd sse2 neon/;

  add_proto qw/void aom_int_pro_row/, "int16_t *hbuf, uint8_t const *ref, const int ref_stride, const int height";
  specialize qw/aom_int_pro_row sse2 neon/;

  add_proto qw/int16_t aom_int_pro_col/, "uint8_t const *ref, const int width";
  specialize qw/aom_int_pro_col sse2 neon/;

  add_proto qw/int aom_vector_var/, "int16_t const *ref, int16_t const *src, const int bwl";
  specialize qw/aom_vector_var neon sse2/;
}  # CONFIG_AV1_ENCODER

add_proto qw/unsigned int aom_sad64x64_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/aom_sad64x64_avg avx2 msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad64x32_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/aom_sad64x32_avg avx2 msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad32x64_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/aom_sad32x64_avg avx2 msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad32x32_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/aom_sad32x32_avg avx2 msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad32x16_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/aom_sad32x16_avg avx2 msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad16x32_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/aom_sad16x32_avg msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad16x16_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/aom_sad16x16_avg msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad16x8_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/aom_sad16x8_avg msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad8x16_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/aom_sad8x16_avg msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad8x8_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/aom_sad8x8_avg msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad8x4_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/aom_sad8x4_avg msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad4x8_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/aom_sad4x8_avg msa/, "$sse2_x86inc";

add_proto qw/unsigned int aom_sad4x4_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
specialize qw/aom_sad4x4_avg msa/, "$sse2_x86inc";

#
# Multi-block SAD, comparing a reference to N blocks 1 pixel apart horizontally
#
# Blocks of 3
add_proto qw/void aom_sad64x64x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad64x64x3 msa/;

add_proto qw/void aom_sad32x32x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad32x32x3 msa/;

add_proto qw/void aom_sad16x16x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad16x16x3 sse3 ssse3 msa/;

add_proto qw/void aom_sad16x8x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad16x8x3 sse3 ssse3 msa/;

add_proto qw/void aom_sad8x16x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad8x16x3 sse3 msa/;

add_proto qw/void aom_sad8x8x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad8x8x3 sse3 msa/;

add_proto qw/void aom_sad4x4x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad4x4x3 sse3 msa/;

# Blocks of 8
add_proto qw/void aom_sad64x64x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad64x64x8 msa/;

add_proto qw/void aom_sad32x32x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad32x32x8 msa/;

add_proto qw/void aom_sad16x16x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad16x16x8 sse4_1 msa/;

add_proto qw/void aom_sad16x8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad16x8x8 sse4_1 msa/;

add_proto qw/void aom_sad8x16x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad8x16x8 sse4_1 msa/;

add_proto qw/void aom_sad8x8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad8x8x8 sse4_1 msa/;

add_proto qw/void aom_sad8x4x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad8x4x8 msa/;

add_proto qw/void aom_sad4x8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad4x8x8 msa/;

add_proto qw/void aom_sad4x4x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad4x4x8 sse4_1 msa/;

#
# Multi-block SAD, comparing a reference to N independent blocks
#
add_proto qw/void aom_sad64x64x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad64x64x4d avx2 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_sad64x32x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad64x32x4d msa/, "$sse2_x86inc";

add_proto qw/void aom_sad32x64x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad32x64x4d msa/, "$sse2_x86inc";

add_proto qw/void aom_sad32x32x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad32x32x4d avx2 neon msa/, "$sse2_x86inc";

add_proto qw/void aom_sad32x16x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad32x16x4d msa/, "$sse2_x86inc";

add_proto qw/void aom_sad16x32x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad16x32x4d msa/, "$sse2_x86inc";

add_proto qw/void aom_sad16x16x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad16x16x4d neon msa/, "$sse2_x86inc";

add_proto qw/void aom_sad16x8x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad16x8x4d msa/, "$sse2_x86inc";

add_proto qw/void aom_sad8x16x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad8x16x4d msa/, "$sse2_x86inc";

add_proto qw/void aom_sad8x8x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad8x8x4d msa/, "$sse2_x86inc";

add_proto qw/void aom_sad8x4x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad8x4x4d msa/, "$sse2_x86inc";

add_proto qw/void aom_sad4x8x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad4x8x4d msa/, "$sse_x86inc";

add_proto qw/void aom_sad4x4x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t * const ref_ptr[], int ref_stride, uint32_t *sad_array";
specialize qw/aom_sad4x4x4d msa/, "$sse_x86inc";

#
# Structured Similarity (SSIM)
#
if (aom_config("CONFIG_INTERNAL_STATS") eq "yes") {
    add_proto qw/void aom_ssim_parms_8x8/, "const uint8_t *s, int sp, const uint8_t *r, int rp, uint32_t *sum_s, uint32_t *sum_r, uint32_t *sum_sq_s, uint32_t *sum_sq_r, uint32_t *sum_sxr";
    specialize qw/aom_ssim_parms_8x8/, "$sse2_x86_64";

    add_proto qw/void aom_ssim_parms_16x16/, "const uint8_t *s, int sp, const uint8_t *r, int rp, uint32_t *sum_s, uint32_t *sum_r, uint32_t *sum_sq_s, uint32_t *sum_sq_r, uint32_t *sum_sxr";
    specialize qw/aom_ssim_parms_16x16/, "$sse2_x86_64";
}

if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
  #
  # Block subtraction
  #
  add_proto qw/void aom_highbd_subtract_block/, "int rows, int cols, int16_t *diff_ptr, ptrdiff_t diff_stride, const uint8_t *src_ptr, ptrdiff_t src_stride, const uint8_t *pred_ptr, ptrdiff_t pred_stride, int bd";
  specialize qw/aom_highbd_subtract_block/;

  #
  # Single block SAD
  #
  add_proto qw/unsigned int aom_highbd_sad64x64/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/aom_highbd_sad64x64/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad64x32/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/aom_highbd_sad64x32/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad32x64/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/aom_highbd_sad32x64/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad32x32/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/aom_highbd_sad32x32/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad32x16/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/aom_highbd_sad32x16/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad16x32/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/aom_highbd_sad16x32/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad16x16/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/aom_highbd_sad16x16/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad16x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/aom_highbd_sad16x8/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad8x16/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/aom_highbd_sad8x16/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/aom_highbd_sad8x8/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad8x4/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/aom_highbd_sad8x4/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad4x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/aom_highbd_sad4x8/;

  add_proto qw/unsigned int aom_highbd_sad4x4/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride";
  specialize qw/aom_highbd_sad4x4/;

  #
  # Avg
  #
  add_proto qw/unsigned int aom_highbd_avg_8x8/, "const uint8_t *, int p";
  specialize qw/aom_highbd_avg_8x8/;
  add_proto qw/unsigned int aom_highbd_avg_4x4/, "const uint8_t *, int p";
  specialize qw/aom_highbd_avg_4x4/;
  add_proto qw/void aom_highbd_minmax_8x8/, "const uint8_t *s, int p, const uint8_t *d, int dp, int *min, int *max";
  specialize qw/aom_highbd_minmax_8x8/;

  add_proto qw/unsigned int aom_highbd_sad64x64_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/aom_highbd_sad64x64_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad64x32_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/aom_highbd_sad64x32_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad32x64_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/aom_highbd_sad32x64_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad32x32_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/aom_highbd_sad32x32_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad32x16_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/aom_highbd_sad32x16_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad16x32_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/aom_highbd_sad16x32_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad16x16_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/aom_highbd_sad16x16_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad16x8_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/aom_highbd_sad16x8_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad8x16_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/aom_highbd_sad8x16_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad8x8_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/aom_highbd_sad8x8_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad8x4_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/aom_highbd_sad8x4_avg/, "$sse2_x86inc";

  add_proto qw/unsigned int aom_highbd_sad4x8_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/aom_highbd_sad4x8_avg/;

  add_proto qw/unsigned int aom_highbd_sad4x4_avg/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, const uint8_t *second_pred";
  specialize qw/aom_highbd_sad4x4_avg/;

  #
  # Multi-block SAD, comparing a reference to N blocks 1 pixel apart horizontally
  #
  # Blocks of 3
  add_proto qw/void aom_highbd_sad64x64x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad64x64x3/;

  add_proto qw/void aom_highbd_sad32x32x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad32x32x3/;

  add_proto qw/void aom_highbd_sad16x16x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad16x16x3/;

  add_proto qw/void aom_highbd_sad16x8x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad16x8x3/;

  add_proto qw/void aom_highbd_sad8x16x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad8x16x3/;

  add_proto qw/void aom_highbd_sad8x8x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad8x8x3/;

  add_proto qw/void aom_highbd_sad4x4x3/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad4x4x3/;

  # Blocks of 8
  add_proto qw/void aom_highbd_sad64x64x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad64x64x8/;

  add_proto qw/void aom_highbd_sad32x32x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad32x32x8/;

  add_proto qw/void aom_highbd_sad16x16x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad16x16x8/;

  add_proto qw/void aom_highbd_sad16x8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad16x8x8/;

  add_proto qw/void aom_highbd_sad8x16x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad8x16x8/;

  add_proto qw/void aom_highbd_sad8x8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad8x8x8/;

  add_proto qw/void aom_highbd_sad8x4x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad8x4x8/;

  add_proto qw/void aom_highbd_sad4x8x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad4x8x8/;

  add_proto qw/void aom_highbd_sad4x4x8/, "const uint8_t *src_ptr, int src_stride, const uint8_t *ref_ptr, int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad4x4x8/;

  #
  # Multi-block SAD, comparing a reference to N independent blocks
  #
  add_proto qw/void aom_highbd_sad64x64x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad64x64x4d/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_sad64x32x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad64x32x4d/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_sad32x64x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad32x64x4d/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_sad32x32x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad32x32x4d/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_sad32x16x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad32x16x4d/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_sad16x32x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad16x32x4d/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_sad16x16x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad16x16x4d/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_sad16x8x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad16x8x4d/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_sad8x16x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad8x16x4d/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_sad8x8x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad8x8x4d/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_sad8x4x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad8x4x4d/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_sad4x8x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad4x8x4d/, "$sse2_x86inc";

  add_proto qw/void aom_highbd_sad4x4x4d/, "const uint8_t *src_ptr, int src_stride, const uint8_t* const ref_ptr[], int ref_stride, uint32_t *sad_array";
  specialize qw/aom_highbd_sad4x4x4d/, "$sse2_x86inc";

  #
  # Structured Similarity (SSIM)
  #
  if (aom_config("CONFIG_INTERNAL_STATS") eq "yes") {
    add_proto qw/void aom_highbd_ssim_parms_8x8/, "const uint16_t *s, int sp, const uint16_t *r, int rp, uint32_t *sum_s, uint32_t *sum_r, uint32_t *sum_sq_s, uint32_t *sum_sq_r, uint32_t *sum_sxr";
    specialize qw/aom_highbd_ssim_parms_8x8/;
  }
}  # CONFIG_AOM_HIGHBITDEPTH
}  # CONFIG_ENCODERS

if (aom_config("CONFIG_ENCODERS") eq "yes") {

#
# Variance
#
add_proto qw/unsigned int aom_variance64x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_variance64x64 sse2 avx2 neon msa/;

add_proto qw/unsigned int aom_variance64x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_variance64x32 sse2 avx2 neon msa/;

add_proto qw/unsigned int aom_variance32x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_variance32x64 sse2 neon msa/;

add_proto qw/unsigned int aom_variance32x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_variance32x32 sse2 avx2 neon msa/;

add_proto qw/unsigned int aom_variance32x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_variance32x16 sse2 avx2 msa/;

add_proto qw/unsigned int aom_variance16x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_variance16x32 sse2 msa/;

add_proto qw/unsigned int aom_variance16x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_variance16x16 mmx sse2 avx2 media neon msa/;

add_proto qw/unsigned int aom_variance16x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_variance16x8 mmx sse2 neon msa/;

add_proto qw/unsigned int aom_variance8x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_variance8x16 mmx sse2 neon msa/;

add_proto qw/unsigned int aom_variance8x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_variance8x8 mmx sse2 media neon msa/;

add_proto qw/unsigned int aom_variance8x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_variance8x4 sse2 msa/;

add_proto qw/unsigned int aom_variance4x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_variance4x8 sse2 msa/;

add_proto qw/unsigned int aom_variance4x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_variance4x4 mmx sse2 msa/;

#
# Specialty Variance
#
add_proto qw/void aom_get16x16var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
  specialize qw/aom_get16x16var sse2 avx2 neon msa/;

add_proto qw/void aom_get8x8var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
  specialize qw/aom_get8x8var mmx sse2 neon msa/;

add_proto qw/unsigned int aom_mse16x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/aom_mse16x16 mmx sse2 avx2 media neon msa/;

add_proto qw/unsigned int aom_mse16x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/aom_mse16x8 sse2 msa/;

add_proto qw/unsigned int aom_mse8x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/aom_mse8x16 sse2 msa/;

add_proto qw/unsigned int aom_mse8x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/aom_mse8x8 sse2 msa/;

add_proto qw/unsigned int aom_get_mb_ss/, "const int16_t *";
  specialize qw/aom_get_mb_ss mmx sse2 msa/;

add_proto qw/unsigned int aom_get4x4sse_cs/, "const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride";
  specialize qw/aom_get4x4sse_cs neon msa/;

add_proto qw/void aom_comp_avg_pred/, "uint8_t *comp_pred, const uint8_t *pred, int width, int height, const uint8_t *ref, int ref_stride";

#
# Subpixel Variance
#
add_proto qw/uint32_t aom_sub_pixel_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_sub_pixel_variance64x64 avx2 neon msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_sub_pixel_variance64x32 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_sub_pixel_variance32x64 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_sub_pixel_variance32x32 avx2 neon msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_sub_pixel_variance32x16 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_sub_pixel_variance16x32 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_sub_pixel_variance16x16 mmx media neon msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_sub_pixel_variance16x8 mmx msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_sub_pixel_variance8x16 mmx msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_sub_pixel_variance8x8 mmx media neon msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_sub_pixel_variance8x4 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_sub_pixel_variance4x8 msa/, "$sse_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_sub_pixel_variance4x4 mmx msa/, "$sse_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_avg_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance64x64 avx2 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_avg_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance64x32 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_avg_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance32x64 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_avg_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance32x32 avx2 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_avg_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance32x16 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_avg_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance16x32 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_avg_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance16x16 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_avg_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance16x8 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_avg_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance8x16 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_avg_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance8x8 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_avg_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance8x4 msa/, "$sse2_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_avg_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance4x8 msa/, "$sse_x86inc", "$ssse3_x86inc";

add_proto qw/uint32_t aom_sub_pixel_avg_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_sub_pixel_avg_variance4x4 msa/, "$sse_x86inc", "$ssse3_x86inc";

#
# Specialty Subpixel
#
add_proto qw/uint32_t aom_variance_halfpixvar16x16_h/, "const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse";
  specialize qw/aom_variance_halfpixvar16x16_h mmx sse2 media/;

add_proto qw/uint32_t aom_variance_halfpixvar16x16_v/, "const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse";
  specialize qw/aom_variance_halfpixvar16x16_v mmx sse2 media/;

add_proto qw/uint32_t aom_variance_halfpixvar16x16_hv/, "const unsigned char *src_ptr, int source_stride, const unsigned char *ref_ptr, int  ref_stride, uint32_t *sse";
  specialize qw/aom_variance_halfpixvar16x16_hv mmx sse2 media/;

if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
  add_proto qw/unsigned int aom_highbd_12_variance64x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_12_variance64x64 sse2/;

  add_proto qw/unsigned int aom_highbd_12_variance64x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_12_variance64x32 sse2/;

  add_proto qw/unsigned int aom_highbd_12_variance32x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_12_variance32x64 sse2/;

  add_proto qw/unsigned int aom_highbd_12_variance32x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_12_variance32x32 sse2/;

  add_proto qw/unsigned int aom_highbd_12_variance32x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_12_variance32x16 sse2/;

  add_proto qw/unsigned int aom_highbd_12_variance16x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_12_variance16x32 sse2/;

  add_proto qw/unsigned int aom_highbd_12_variance16x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_12_variance16x16 sse2/;

  add_proto qw/unsigned int aom_highbd_12_variance16x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_12_variance16x8 sse2/;

  add_proto qw/unsigned int aom_highbd_12_variance8x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_12_variance8x16 sse2/;

  add_proto qw/unsigned int aom_highbd_12_variance8x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_12_variance8x8 sse2/;

  add_proto qw/unsigned int aom_highbd_12_variance8x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_highbd_12_variance4x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_highbd_12_variance4x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

  add_proto qw/unsigned int aom_highbd_10_variance64x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_10_variance64x64 sse2/;

  add_proto qw/unsigned int aom_highbd_10_variance64x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_10_variance64x32 sse2/;

  add_proto qw/unsigned int aom_highbd_10_variance32x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_10_variance32x64 sse2/;

  add_proto qw/unsigned int aom_highbd_10_variance32x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_10_variance32x32 sse2/;

  add_proto qw/unsigned int aom_highbd_10_variance32x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_10_variance32x16 sse2/;

  add_proto qw/unsigned int aom_highbd_10_variance16x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_10_variance16x32 sse2/;

  add_proto qw/unsigned int aom_highbd_10_variance16x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_10_variance16x16 sse2/;

  add_proto qw/unsigned int aom_highbd_10_variance16x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_10_variance16x8 sse2/;

  add_proto qw/unsigned int aom_highbd_10_variance8x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_10_variance8x16 sse2/;

  add_proto qw/unsigned int aom_highbd_10_variance8x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_10_variance8x8 sse2/;

  add_proto qw/unsigned int aom_highbd_10_variance8x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_highbd_10_variance4x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_highbd_10_variance4x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

  add_proto qw/unsigned int aom_highbd_8_variance64x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_8_variance64x64 sse2/;

  add_proto qw/unsigned int aom_highbd_8_variance64x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_8_variance64x32 sse2/;

  add_proto qw/unsigned int aom_highbd_8_variance32x64/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_8_variance32x64 sse2/;

  add_proto qw/unsigned int aom_highbd_8_variance32x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_8_variance32x32 sse2/;

  add_proto qw/unsigned int aom_highbd_8_variance32x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_8_variance32x16 sse2/;

  add_proto qw/unsigned int aom_highbd_8_variance16x32/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_8_variance16x32 sse2/;

  add_proto qw/unsigned int aom_highbd_8_variance16x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_8_variance16x16 sse2/;

  add_proto qw/unsigned int aom_highbd_8_variance16x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_8_variance16x8 sse2/;

  add_proto qw/unsigned int aom_highbd_8_variance8x16/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_8_variance8x16 sse2/;

  add_proto qw/unsigned int aom_highbd_8_variance8x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  specialize qw/aom_highbd_8_variance8x8 sse2/;

  add_proto qw/unsigned int aom_highbd_8_variance8x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_highbd_8_variance4x8/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_highbd_8_variance4x4/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse";

  add_proto qw/void aom_highbd_8_get16x16var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
  add_proto qw/void aom_highbd_8_get8x8var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";

  add_proto qw/void aom_highbd_10_get16x16var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
  add_proto qw/void aom_highbd_10_get8x8var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";

  add_proto qw/void aom_highbd_12_get16x16var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";
  add_proto qw/void aom_highbd_12_get8x8var/, "const uint8_t *src_ptr, int source_stride, const uint8_t *ref_ptr, int ref_stride, unsigned int *sse, int *sum";

  add_proto qw/unsigned int aom_highbd_8_mse16x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/aom_highbd_8_mse16x16 sse2/;

  add_proto qw/unsigned int aom_highbd_8_mse16x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_highbd_8_mse8x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_highbd_8_mse8x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/aom_highbd_8_mse8x8 sse2/;

  add_proto qw/unsigned int aom_highbd_10_mse16x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/aom_highbd_10_mse16x16 sse2/;

  add_proto qw/unsigned int aom_highbd_10_mse16x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_highbd_10_mse8x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_highbd_10_mse8x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/aom_highbd_10_mse8x8 sse2/;

  add_proto qw/unsigned int aom_highbd_12_mse16x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/aom_highbd_12_mse16x16 sse2/;

  add_proto qw/unsigned int aom_highbd_12_mse16x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_highbd_12_mse8x16/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  add_proto qw/unsigned int aom_highbd_12_mse8x8/, "const uint8_t *src_ptr, int  source_stride, const uint8_t *ref_ptr, int  recon_stride, unsigned int *sse";
  specialize qw/aom_highbd_12_mse8x8 sse2/;

  add_proto qw/void aom_highbd_comp_avg_pred/, "uint16_t *comp_pred, const uint8_t *pred8, int width, int height, const uint8_t *ref8, int ref_stride";

  #
  # Subpixel Variance
  #
  add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_12_sub_pixel_variance64x64/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_12_sub_pixel_variance64x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_12_sub_pixel_variance32x64/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_12_sub_pixel_variance32x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_12_sub_pixel_variance32x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_12_sub_pixel_variance16x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_12_sub_pixel_variance16x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_12_sub_pixel_variance16x8/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_12_sub_pixel_variance8x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_12_sub_pixel_variance8x8/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_12_sub_pixel_variance8x4/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  add_proto qw/uint32_t aom_highbd_12_sub_pixel_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_10_sub_pixel_variance64x64/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_10_sub_pixel_variance64x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_10_sub_pixel_variance32x64/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_10_sub_pixel_variance32x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_10_sub_pixel_variance32x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_10_sub_pixel_variance16x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_10_sub_pixel_variance16x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_10_sub_pixel_variance16x8/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_10_sub_pixel_variance8x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_10_sub_pixel_variance8x8/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_10_sub_pixel_variance8x4/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  add_proto qw/uint32_t aom_highbd_10_sub_pixel_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_8_sub_pixel_variance64x64/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_8_sub_pixel_variance64x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_8_sub_pixel_variance32x64/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_8_sub_pixel_variance32x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_8_sub_pixel_variance32x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_8_sub_pixel_variance16x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_8_sub_pixel_variance16x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_8_sub_pixel_variance16x8/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_8_sub_pixel_variance8x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_8_sub_pixel_variance8x8/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  specialize qw/aom_highbd_8_sub_pixel_variance8x4/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";
  add_proto qw/uint32_t aom_highbd_8_sub_pixel_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_12_sub_pixel_avg_variance64x64/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_12_sub_pixel_avg_variance64x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_12_sub_pixel_avg_variance32x64/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_12_sub_pixel_avg_variance32x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_12_sub_pixel_avg_variance32x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_12_sub_pixel_avg_variance16x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_12_sub_pixel_avg_variance16x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_12_sub_pixel_avg_variance16x8/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_12_sub_pixel_avg_variance8x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_12_sub_pixel_avg_variance8x8/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_12_sub_pixel_avg_variance8x4/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  add_proto qw/uint32_t aom_highbd_12_sub_pixel_avg_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_10_sub_pixel_avg_variance64x64/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_10_sub_pixel_avg_variance64x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_10_sub_pixel_avg_variance32x64/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_10_sub_pixel_avg_variance32x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_10_sub_pixel_avg_variance32x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_10_sub_pixel_avg_variance16x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_10_sub_pixel_avg_variance16x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_10_sub_pixel_avg_variance16x8/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_10_sub_pixel_avg_variance8x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_10_sub_pixel_avg_variance8x8/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_10_sub_pixel_avg_variance8x4/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  add_proto qw/uint32_t aom_highbd_10_sub_pixel_avg_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance64x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_8_sub_pixel_avg_variance64x64/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance64x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_8_sub_pixel_avg_variance64x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance32x64/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_8_sub_pixel_avg_variance32x64/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance32x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_8_sub_pixel_avg_variance32x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance32x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_8_sub_pixel_avg_variance32x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance16x32/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_8_sub_pixel_avg_variance16x32/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance16x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_8_sub_pixel_avg_variance16x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance16x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_8_sub_pixel_avg_variance16x8/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance8x16/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_8_sub_pixel_avg_variance8x16/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance8x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_8_sub_pixel_avg_variance8x8/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance8x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  specialize qw/aom_highbd_8_sub_pixel_avg_variance8x4/, "$sse2_x86inc";

  add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance4x8/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";
  add_proto qw/uint32_t aom_highbd_8_sub_pixel_avg_variance4x4/, "const uint8_t *src_ptr, int source_stride, int xoffset, int  yoffset, const uint8_t *ref_ptr, int ref_stride, uint32_t *sse, const uint8_t *second_pred";

}  # CONFIG_AOM_HIGHBITDEPTH
}  # CONFIG_ENCODERS

1;
