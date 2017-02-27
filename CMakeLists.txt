##
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
cmake_minimum_required(VERSION 3.2)
project(AOM C CXX)

set(AOM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(AOM_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}")

include("${AOM_ROOT}/build/cmake/aom_configure.cmake")
include("${AOM_ROOT}/build/cmake/aom_optimization.cmake")
include("${AOM_ROOT}/aom_dsp/aom_dsp.cmake")
include("${AOM_ROOT}/test/test.cmake")

set(AOM_RTCD_SOURCES
    "${AOM_CONFIG_DIR}/aom_dsp_rtcd.h"
    "${AOM_CONFIG_DIR}/aom_scale_rtcd.h"
    "${AOM_CONFIG_DIR}/av1_rtcd.h"
    "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd_defs.pl"
    "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd.c"
    "${AOM_ROOT}/aom_scale/aom_scale_rtcd.pl"
    "${AOM_ROOT}/aom_scale/aom_scale_rtcd.c"
    "${AOM_ROOT}/av1/common/av1_rtcd_defs.pl"
    "${AOM_ROOT}/av1/common/av1_rtcd.c"
    "${AOM_ROOT}/build/make/rtcd.pl")

# TODO(tomfinegan): Use libwebm's cmake support directly.
set(AOM_LIBWEBM_SOURCES
    "${AOM_ROOT}/third_party/libwebm/common/hdr_util.cc"
    "${AOM_ROOT}/third_party/libwebm/common/hdr_util.h"
    "${AOM_ROOT}/third_party/libwebm/common/webmids.h"
    "${AOM_ROOT}/third_party/libwebm/mkvmuxer/mkvmuxer.cc"
    "${AOM_ROOT}/third_party/libwebm/mkvmuxer/mkvmuxer.h"
    "${AOM_ROOT}/third_party/libwebm/mkvmuxer/mkvmuxertypes.h"
    "${AOM_ROOT}/third_party/libwebm/mkvmuxer/mkvmuxerutil.cc"
    "${AOM_ROOT}/third_party/libwebm/mkvmuxer/mkvmuxerutil.h"
    "${AOM_ROOT}/third_party/libwebm/mkvmuxer/mkvwriter.cc"
    "${AOM_ROOT}/third_party/libwebm/mkvmuxer/mkvwriter.h"
    "${AOM_ROOT}/third_party/libwebm/mkvparser/mkvparser.cc"
    "${AOM_ROOT}/third_party/libwebm/mkvparser/mkvparser.h"
    "${AOM_ROOT}/third_party/libwebm/mkvparser/mkvreader.cc"
    "${AOM_ROOT}/third_party/libwebm/mkvparser/mkvreader.h")

set(AOM_LIBYUV_SOURCES
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/basic_types.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/convert.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/convert_argb.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/convert_from.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/cpu_id.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/planar_functions.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/rotate.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/row.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/scale.h"
    "${AOM_ROOT}/third_party/libyuv/include/libyuv/scale_row.h"
    "${AOM_ROOT}/third_party/libyuv/source/cpu_id.cc"
    "${AOM_ROOT}/third_party/libyuv/source/planar_functions.cc"
    "${AOM_ROOT}/third_party/libyuv/source/row_any.cc"
    "${AOM_ROOT}/third_party/libyuv/source/row_common.cc"
    "${AOM_ROOT}/third_party/libyuv/source/row_gcc.cc"
    "${AOM_ROOT}/third_party/libyuv/source/row_mips.cc"
    "${AOM_ROOT}/third_party/libyuv/source/row_neon.cc"
    "${AOM_ROOT}/third_party/libyuv/source/row_neon64.cc"
    "${AOM_ROOT}/third_party/libyuv/source/row_win.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale_any.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale_common.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale_gcc.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale_mips.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale_neon.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale_neon64.cc"
    "${AOM_ROOT}/third_party/libyuv/source/scale_win.cc")

set(AOM_SOURCES
    "${AOM_CONFIG_DIR}/aom_config.c"
    "${AOM_CONFIG_DIR}/aom_config.h"
    "${AOM_ROOT}/aom/aom.h"
    "${AOM_ROOT}/aom/aom_codec.h"
    "${AOM_ROOT}/aom/aom_decoder.h"
    "${AOM_ROOT}/aom/aom_encoder.h"
    "${AOM_ROOT}/aom/aom_frame_buffer.h"
    "${AOM_ROOT}/aom/aom_image.h"
    "${AOM_ROOT}/aom/aom_integer.h"
    "${AOM_ROOT}/aom/aomcx.h"
    "${AOM_ROOT}/aom/aomdx.h"
    "${AOM_ROOT}/aom/internal/aom_codec_internal.h"
    "${AOM_ROOT}/aom/src/aom_codec.c"
    "${AOM_ROOT}/aom/src/aom_decoder.c"
    "${AOM_ROOT}/aom/src/aom_encoder.c"
    "${AOM_ROOT}/aom/src/aom_image.c")

set(AOM_MEM_SOURCES
    "${AOM_ROOT}/aom_mem/aom_mem.c"
    "${AOM_ROOT}/aom_mem/aom_mem.h"
    "${AOM_ROOT}/aom_mem/include/aom_mem_intrnl.h")

set(AOM_SCALE_SOURCES
    "${AOM_ROOT}/aom_scale/aom_scale.h"
    "${AOM_ROOT}/aom_scale/generic/aom_scale.c"
    "${AOM_ROOT}/aom_scale/generic/gen_scalers.c"
    "${AOM_ROOT}/aom_scale/generic/yv12config.c"
    "${AOM_ROOT}/aom_scale/generic/yv12extend.c"
    "${AOM_ROOT}/aom_scale/yv12config.h")

set(AOM_UTIL_SOURCES
    "${AOM_ROOT}/aom_ports/aom_once.h"
    "${AOM_ROOT}/aom_ports/aom_timer.h"
    "${AOM_ROOT}/aom_ports/bitops.h"
    "${AOM_ROOT}/aom_ports/emmintrin_compat.h"
    "${AOM_ROOT}/aom_ports/mem.h"
    "${AOM_ROOT}/aom_ports/mem_ops.h"
    "${AOM_ROOT}/aom_ports/mem_ops_aligned.h"
    "${AOM_ROOT}/aom_ports/msvc.h"
    "${AOM_ROOT}/aom_ports/system_state.h"
    "${AOM_ROOT}/aom_util/aom_thread.c"
    "${AOM_ROOT}/aom_util/aom_thread.h"
    "${AOM_ROOT}/aom_util/endian_inl.h")

set(AOM_AV1_COMMON_SOURCES
    "${AOM_ROOT}/av1/av1_iface_common.h"
    "${AOM_ROOT}/av1/common/alloccommon.c"
    "${AOM_ROOT}/av1/common/alloccommon.h"
    "${AOM_ROOT}/av1/common/av1_fwd_txfm1d.c"
    "${AOM_ROOT}/av1/common/av1_fwd_txfm1d.h"
    "${AOM_ROOT}/av1/common/av1_fwd_txfm2d.c"
    "${AOM_ROOT}/av1/common/av1_fwd_txfm2d_cfg.h"
    "${AOM_ROOT}/av1/common/av1_inv_txfm1d.c"
    "${AOM_ROOT}/av1/common/av1_inv_txfm1d.h"
    "${AOM_ROOT}/av1/common/av1_inv_txfm2d.c"
    "${AOM_ROOT}/av1/common/av1_inv_txfm2d_cfg.h"
    "${AOM_ROOT}/av1/common/av1_loopfilter.c"
    "${AOM_ROOT}/av1/common/av1_loopfilter.h"
    "${AOM_ROOT}/av1/common/av1_txfm.h"
    "${AOM_ROOT}/av1/common/blockd.c"
    "${AOM_ROOT}/av1/common/blockd.h"
    "${AOM_ROOT}/av1/common/common.h"
    "${AOM_ROOT}/av1/common/common_data.h"
    "${AOM_ROOT}/av1/common/convolve.c"
    "${AOM_ROOT}/av1/common/convolve.h"
    "${AOM_ROOT}/av1/common/debugmodes.c"
    "${AOM_ROOT}/av1/common/entropy.c"
    "${AOM_ROOT}/av1/common/entropy.h"
    "${AOM_ROOT}/av1/common/entropymode.c"
    "${AOM_ROOT}/av1/common/entropymode.h"
    "${AOM_ROOT}/av1/common/entropymv.c"
    "${AOM_ROOT}/av1/common/entropymv.h"
    "${AOM_ROOT}/av1/common/enums.h"
    "${AOM_ROOT}/av1/common/filter.c"
    "${AOM_ROOT}/av1/common/filter.h"
    "${AOM_ROOT}/av1/common/frame_buffers.c"
    "${AOM_ROOT}/av1/common/frame_buffers.h"
    "${AOM_ROOT}/av1/common/idct.c"
    "${AOM_ROOT}/av1/common/idct.h"
    "${AOM_ROOT}/av1/common/mv.h"
    "${AOM_ROOT}/av1/common/mvref_common.c"
    "${AOM_ROOT}/av1/common/mvref_common.h"
    "${AOM_ROOT}/av1/common/odintrin.c"
    "${AOM_ROOT}/av1/common/odintrin.h"
    "${AOM_ROOT}/av1/common/onyxc_int.h"
    "${AOM_ROOT}/av1/common/pred_common.c"
    "${AOM_ROOT}/av1/common/pred_common.h"
    "${AOM_ROOT}/av1/common/quant_common.c"
    "${AOM_ROOT}/av1/common/quant_common.h"
    "${AOM_ROOT}/av1/common/reconinter.c"
    "${AOM_ROOT}/av1/common/reconinter.h"
    "${AOM_ROOT}/av1/common/reconintra.c"
    "${AOM_ROOT}/av1/common/reconintra.h"
    "${AOM_ROOT}/av1/common/restoration.h"
    "${AOM_ROOT}/av1/common/scale.c"
    "${AOM_ROOT}/av1/common/scale.h"
    "${AOM_ROOT}/av1/common/scan.c"
    "${AOM_ROOT}/av1/common/scan.h"
    "${AOM_ROOT}/av1/common/seg_common.c"
    "${AOM_ROOT}/av1/common/seg_common.h"
    "${AOM_ROOT}/av1/common/thread_common.c"
    "${AOM_ROOT}/av1/common/thread_common.h"
    "${AOM_ROOT}/av1/common/tile_common.c"
    "${AOM_ROOT}/av1/common/tile_common.h")

set(AOM_AV1_DECODER_SOURCES
    "${AOM_ROOT}/av1/av1_dx_iface.c"
    "${AOM_ROOT}/av1/decoder/decodeframe.c"
    "${AOM_ROOT}/av1/decoder/decodeframe.h"
    "${AOM_ROOT}/av1/decoder/decodemv.c"
    "${AOM_ROOT}/av1/decoder/decodemv.h"
    "${AOM_ROOT}/av1/decoder/decoder.c"
    "${AOM_ROOT}/av1/decoder/decoder.h"
    "${AOM_ROOT}/av1/decoder/detokenize.c"
    "${AOM_ROOT}/av1/decoder/detokenize.h"
    "${AOM_ROOT}/av1/decoder/dsubexp.c"
    "${AOM_ROOT}/av1/decoder/dsubexp.h"
    "${AOM_ROOT}/av1/decoder/dthread.c"
    "${AOM_ROOT}/av1/decoder/dthread.h")

set(AOM_AV1_ENCODER_SOURCES
    "${AOM_ROOT}/av1/av1_cx_iface.c"
    "${AOM_ROOT}/av1/encoder/aq_complexity.c"
    "${AOM_ROOT}/av1/encoder/aq_complexity.h"
    "${AOM_ROOT}/av1/encoder/aq_cyclicrefresh.c"
    "${AOM_ROOT}/av1/encoder/aq_cyclicrefresh.h"
    "${AOM_ROOT}/av1/encoder/aq_variance.c"
    "${AOM_ROOT}/av1/encoder/aq_variance.h"
    "${AOM_ROOT}/av1/encoder/av1_quantize.c"
    "${AOM_ROOT}/av1/encoder/av1_quantize.h"
    "${AOM_ROOT}/av1/encoder/bitstream.c"
    "${AOM_ROOT}/av1/encoder/bitstream.h"
    "${AOM_ROOT}/av1/encoder/block.h"
    "${AOM_ROOT}/av1/encoder/context_tree.c"
    "${AOM_ROOT}/av1/encoder/context_tree.h"
    "${AOM_ROOT}/av1/encoder/cost.c"
    "${AOM_ROOT}/av1/encoder/cost.h"
    "${AOM_ROOT}/av1/encoder/dct.c"
    "${AOM_ROOT}/av1/encoder/encodeframe.c"
    "${AOM_ROOT}/av1/encoder/encodeframe.h"
    "${AOM_ROOT}/av1/encoder/encodemb.c"
    "${AOM_ROOT}/av1/encoder/encodemb.h"
    "${AOM_ROOT}/av1/encoder/encodemv.c"
    "${AOM_ROOT}/av1/encoder/encodemv.h"
    "${AOM_ROOT}/av1/encoder/encoder.c"
    "${AOM_ROOT}/av1/encoder/encoder.h"
    "${AOM_ROOT}/av1/encoder/ethread.c"
    "${AOM_ROOT}/av1/encoder/ethread.h"
    "${AOM_ROOT}/av1/encoder/extend.c"
    "${AOM_ROOT}/av1/encoder/extend.h"
    "${AOM_ROOT}/av1/encoder/firstpass.c"
    "${AOM_ROOT}/av1/encoder/firstpass.h"
    "${AOM_ROOT}/av1/encoder/hybrid_fwd_txfm.c"
    "${AOM_ROOT}/av1/encoder/hybrid_fwd_txfm.h"
    "${AOM_ROOT}/av1/encoder/lookahead.c"
    "${AOM_ROOT}/av1/encoder/lookahead.h"
    "${AOM_ROOT}/av1/encoder/mbgraph.c"
    "${AOM_ROOT}/av1/encoder/mbgraph.h"
    "${AOM_ROOT}/av1/encoder/mcomp.c"
    "${AOM_ROOT}/av1/encoder/mcomp.h"
    "${AOM_ROOT}/av1/encoder/picklpf.c"
    "${AOM_ROOT}/av1/encoder/picklpf.h"
    "${AOM_ROOT}/av1/encoder/ratectrl.c"
    "${AOM_ROOT}/av1/encoder/ratectrl.h"
    "${AOM_ROOT}/av1/encoder/rd.c"
    "${AOM_ROOT}/av1/encoder/rd.h"
    "${AOM_ROOT}/av1/encoder/rdopt.c"
    "${AOM_ROOT}/av1/encoder/rdopt.h"
    "${AOM_ROOT}/av1/encoder/resize.c"
    "${AOM_ROOT}/av1/encoder/resize.h"
    "${AOM_ROOT}/av1/encoder/segmentation.c"
    "${AOM_ROOT}/av1/encoder/segmentation.h"
    "${AOM_ROOT}/av1/encoder/speed_features.c"
    "${AOM_ROOT}/av1/encoder/speed_features.h"
    "${AOM_ROOT}/av1/encoder/subexp.c"
    "${AOM_ROOT}/av1/encoder/subexp.h"
    "${AOM_ROOT}/av1/encoder/temporal_filter.c"
    "${AOM_ROOT}/av1/encoder/temporal_filter.h"
    "${AOM_ROOT}/av1/encoder/tokenize.c"
    "${AOM_ROOT}/av1/encoder/tokenize.h"
    "${AOM_ROOT}/av1/encoder/treewriter.c"
    "${AOM_ROOT}/av1/encoder/treewriter.h"
    "${AOM_ROOT}/av1/encoder/variance_tree.c"
    "${AOM_ROOT}/av1/encoder/variance_tree.h")

set(AOM_COMMON_APP_UTIL_SOURCES
    "${AOM_ROOT}/args.c"
    "${AOM_ROOT}/args.h"
    "${AOM_ROOT}/md5_utils.c"
    "${AOM_ROOT}/md5_utils.h"
    "${AOM_ROOT}/tools_common.c"
    "${AOM_ROOT}/tools_common.h"
    "${AOM_ROOT}/video_common.h")

set(AOM_DECODER_APP_UTIL_SOURCES
    "${AOM_ROOT}/ivfdec.c"
    "${AOM_ROOT}/ivfdec.h"
    "${AOM_ROOT}/video_reader.c"
    "${AOM_ROOT}/video_reader.h"
    "${AOM_ROOT}/y4menc.c"
    "${AOM_ROOT}/y4menc.h")

set(AOM_ENCODER_APP_UTIL_SOURCES
    "${AOM_ROOT}/ivfenc.c"
    "${AOM_ROOT}/ivfenc.h"
    "${AOM_ROOT}/video_writer.c"
    "${AOM_ROOT}/video_writer.h"
    "${AOM_ROOT}/warnings.c"
    "${AOM_ROOT}/warnings.h"
    "${AOM_ROOT}/y4minput.c"
    "${AOM_ROOT}/y4minput.h")

set(AOM_ENCODER_STATS_SOURCES
    "${AOM_ROOT}/aomstats.c"
    "${AOM_ROOT}/aomstats.h"
    "${AOM_ROOT}/rate_hist.c"
    "${AOM_ROOT}/rate_hist.h")

set(AOM_WEBM_DECODER_SOURCES
    "${AOM_ROOT}/webmdec.cc"
    "${AOM_ROOT}/webmdec.h")

set(AOM_WEBM_ENCODER_SOURCES
    "${AOM_ROOT}/webmenc.cc"
    "${AOM_ROOT}/webmenc.h")

set(AOM_PORTS_ASM_X86 "${AOM_ROOT}/aom_ports/x86_abi_support.asm")
set(AOM_PORTS_ASM_MMX "${AOM_ROOT}/aom_ports/emms.asm")

set(AOM_AV1_COMMON_SSE2_INTRIN
    # Requires CONFIG_GLOBAL_MOTION or CONFIG_WARPED_MOTION
    #"${AOM_ROOT}/av1/common/x86/warp_plane_sse2.c"
    "${AOM_ROOT}/av1/common/x86/idct_intrin_sse2.c")

set(AOM_AV1_COMMON_SSSE3_INTRIN
    "${AOM_ROOT}/av1/common/x86/av1_convolve_ssse3.c")

set(AOM_AV1_COMMON_SSE4_1_INTRIN
    # Requires CONFIG_FILTER_INTRA
    #"${AOM_ROOT}/av1/common/x86/filterintra_sse4.c"
    # Requires CONFIG_CDEF
    #"${AOM_ROOT}/av1/common/x86/od_dering_sse4.c"
    "${AOM_ROOT}/av1/common/x86/av1_fwd_txfm1d_sse4.c"
    "${AOM_ROOT}/av1/common/x86/av1_fwd_txfm2d_sse4.c")

set(AOM_AV1_COMMON_AVX2_INTRIN
    "${AOM_ROOT}/av1/common/x86/hybrid_inv_txfm_avx2.c")

set(AOM_AV1_ENCODER_SSE2_ASM
    "${AOM_ROOT}/av1/encoder/x86/dct_sse2.asm"
    "${AOM_ROOT}/av1/encoder/x86/error_sse2.asm"
    "${AOM_ROOT}/av1/encoder/x86/temporal_filter_apply_sse2.asm")

set(AOM_AV1_ENCODER_SSE2_INTRIN
    "${AOM_ROOT}/av1/encoder/x86/dct_intrin_sse2.c"
    "${AOM_ROOT}/av1/encoder/x86/highbd_block_error_intrin_sse2.c"
    "${AOM_ROOT}/av1/encoder/x86/av1_quantize_sse2.c")

set(AOM_AV1_ENCODER_SSSE3_ASM_X86_64
    "${AOM_ROOT}/av1/encoder/x86/av1_quantize_ssse3_x86_64.asm")

set(AOM_AV1_ENCODER_SSSE3_INTRIN
    "${AOM_ROOT}/av1/encoder/x86/dct_ssse3.c")

set(AOM_AV1_ENCODER_AVX2_INTRIN
    "${AOM_ROOT}/av1/encoder/x86/error_intrin_avx2.c"
    "${AOM_ROOT}/av1/encoder/x86/hybrid_fwd_txfm_avx2.c")

if (CONFIG_ACCOUNTING)
  set(AOM_AV1_COMMON_SOURCES
      ${AOM_AV1_COMMON_SOURCES}
      "${AOM_ROOT}/av1/common/accounting.c"
      "${AOM_ROOT}/av1/common/accounting.h")
endif ()

if (CONFIG_CDEF)
  set(AOM_AV1_COMMON_SOURCES
      ${AOM_AV1_COMMON_SOURCES}
      "${AOM_ROOT}/av1/common/clpf.c"
      "${AOM_ROOT}/av1/common/clpf.h")

  set(AOM_AV1_ENCODER_SOURCES
      ${AOM_AV1_ENCODER_SOURCES}
      "${AOM_ROOT}/av1/encoder/clpf_rdo.c"
      "${AOM_ROOT}/av1/encoder/clpf_rdo.h")

  set(AOM_AV1_COMMON_SSE2_INTRIN
      ${AOM_AV1_COMMON_SSE2_INTRIN}
      "${AOM_ROOT}/av1/common/clpf_sse2.c")

  set(AOM_AV1_COMMON_SSSE3_INTRIN
      ${AOM_AV1_COMMON_SSSE3_INTRIN}
      "${AOM_ROOT}/av1/common/clpf_ssse3.c")

  set(AOM_AV1_COMMON_SSE4_1_INTRIN
      ${AOM_AV1_COMMON_SSE4_1_INTRIN}
      "${AOM_ROOT}/av1/common/clpf_sse4.c")

  set(AOM_AV1_ENCODER_SSE2_INTRIN
      ${AOM_AV1_ENCODER_SSE2_INTRIN}
      "${AOM_ROOT}/av1/encoder/clpf_rdo_sse2.c")

  set(AOM_AV1_ENCODER_SSSE3_INTRIN
      ${AOM_AV1_ENCODER_SSSE3_INTRIN}
      "${AOM_ROOT}/av1/encoder/clpf_rdo_ssse3.c")

  set(AOM_AV1_ENCODER_SSE4_1_INTRIN
      ${AOM_AV1_ENCODER_SSE4_1_INTRIN}
      "${AOM_ROOT}/av1/encoder/clpf_rdo_sse4.c")
endif ()

if (CONFIG_EXT_INTER)
  set(AOM_AV1_ENCODER_SOURCES
      ${AOM_AV1_ENCODER_SOURCES}
      "${AOM_ROOT}/av1/encoder/wedge_utils.c")

  set(AOM_AV1_ENCODER_SSE2_INTRIN
      ${AOM_AV1_ENCODER_SSE2_INTRIN}
      "${AOM_ROOT}/av1/encoder/x86/wedge_utils_sse2.c")
endif ()

if (CONFIG_INTERNAL_STATS)
  set(AOM_AV1_ENCODER_SOURCES
      ${AOM_AV1_ENCODER_SOURCES}
      "${AOM_ROOT}/av1/encoder/blockiness.c")
endif ()

if (CONFIG_AOM_HIGHBITDEPTH)
  set(AOM_AV1_COMMON_SSE4_1_INTRIN
      ${AOM_AV1_COMMON_SSE4_1_INTRIN}
      "${AOM_ROOT}/av1/common/x86/av1_highbd_convolve_sse4.c"
      "${AOM_ROOT}/av1/common/x86/highbd_inv_txfm_sse4.c")

  set(AOM_AV1_COMMON_AVX2_INTRIN
      ${AOM_AV1_COMMON_AVX2_INTRIN}
      "${AOM_ROOT}/av1/common/x86/highbd_inv_txfm_avx2.c")

  set(AOM_AV1_ENCODER_SSE4_1_INTRIN
      ${AOM_AV1_ENCODER_SSE4_1_INTRIN}
      "${AOM_ROOT}/av1/encoder/x86/av1_highbd_quantize_sse4.c"
      "${AOM_ROOT}/av1/encoder/x86/highbd_fwd_txfm_sse4.c")
endif ()


include_directories(${AOM_ROOT} ${AOM_CONFIG_DIR})

# Targets
# TODO(tomfinegan): Add support for enable/disable encoder/decoder.
add_rtcd_build_step("${AOM_ROOT}/aom_dsp/aom_dsp_rtcd_defs.pl"
                    "${AOM_CONFIG_DIR}/aom_dsp_rtcd.h"
                    "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd.c"
                    "aom_dsp_rtcd")
add_rtcd_build_step("${AOM_ROOT}/aom_scale/aom_scale_rtcd.pl"
                    "${AOM_CONFIG_DIR}/aom_scale_rtcd.h"
                    "${AOM_ROOT}/aom_scale/aom_scale_rtcd.c"
                    "aom_scale_rtcd")
add_rtcd_build_step("${AOM_ROOT}/av1/common/av1_rtcd_defs.pl"
                    "${AOM_CONFIG_DIR}/av1_rtcd.h"
                    "${AOM_ROOT}/av1/common/av1_rtcd.c"
                    "av1_rtcd")

add_library(aom_rtcd OBJECT ${AOM_RTCD_SOURCES})
add_library(aom_util OBJECT ${AOM_UTIL_SOURCES})
add_library(aom_encoder_stats OBJECT ${AOM_ENCODER_STATS_SOURCES})
add_library(aom_mem OBJECT ${AOM_MEM_SOURCES})
add_library(aom_scale OBJECT ${AOM_SCALE_SOURCES})
add_library(aom_av1_common OBJECT ${AOM_AV1_COMMON_SOURCES})
add_library(aom_av1_decoder OBJECT ${AOM_AV1_DECODER_SOURCES})
add_library(aom_av1_encoder OBJECT ${AOM_AV1_ENCODER_SOURCES})
add_library(aom
            ${AOM_SOURCES}
            $<TARGET_OBJECTS:aom_mem>
            $<TARGET_OBJECTS:aom_rtcd>
            $<TARGET_OBJECTS:aom_scale>
            $<TARGET_OBJECTS:aom_util>
            $<TARGET_OBJECTS:aom_av1_common>
            $<TARGET_OBJECTS:aom_av1_decoder>
            $<TARGET_OBJECTS:aom_av1_encoder>)

# List of object and static library targets.
set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} aom_rtcd aom_util aom_encoder_stats
    aom_mem aom_scale aom_av1_common aom_av1_decoder aom_av1_encoder aom)

#
# Assembly and intrinsic targets.
#
if ("${AOM_TARGET_CPU}" STREQUAL "x86" OR "${AOM_TARGET_CPU}" STREQUAL "x86_64")
  add_asm_library("aom_util_x86" "AOM_PORTS_ASM_X86" "aom")
endif ()

if (HAVE_MMX)
  add_asm_library("aom_util_mmx" "AOM_PORTS_ASM_MMX" "aom")
endif ()

if (HAVE_SSE2)
  add_asm_library("aom_av1_encoder_sse2" "AOM_AV1_ENCODER_SSE2_ASM" "aom")
  require_flag_nomsvc("-msse2" NO)
  add_intrinsics_object_library("-msse2" "sse2" "aom_av1_common"
                                "AOM_AV1_COMMON_SSE2_INTRIN")
  add_intrinsics_object_library("-msse2" "sse2" "aom_av1_encoder"
                                "AOM_AV1_ENCODER_SSE2_INTRIN")
endif ()

if (HAVE_SSSE3)
  require_flag_nomsvc("-mssse3" NO)
  add_intrinsics_object_library("-mssse3" "ssse3" "aom_av1_common"
                                "AOM_AV1_COMMON_SSSE3_INTRIN")
  add_intrinsics_object_library("-mssse3" "ssse3" "aom_av1_encoder"
                                "AOM_AV1_ENCODER_SSSE3_INTRIN")
endif ()

if (HAVE_SSE4_1)
  if ("${AOM_TARGET_CPU}" STREQUAL "x86_64")
    add_asm_library("aom_av1_encoder_ssse3"
                    "AOM_AV1_ENCODER_SSSE3_ASM_X86_64" "aom")
  endif ()
  require_flag_nomsvc("-msse4.1" NO)
  add_intrinsics_object_library("-msse4.1" "sse4" "aom_av1_common"
                                "AOM_AV1_COMMON_SSE4_1_INTRIN")
  if (AOM_AV1_ENCODER_SSE4_1_INTRIN)
    add_intrinsics_object_library("-msse4.1" "sse4" "aom_av1_encoder"
                                  "AOM_AV1_ENCODER_SSE4_1_INTRIN")
  endif ()
endif ()

if (HAVE_AVX2)
  require_flag_nomsvc("-mavx2" NO)
  add_intrinsics_object_library("-mavx2" "avx2" "aom_av1_common"
                                "AOM_AV1_COMMON_AVX2_INTRIN")
  add_intrinsics_object_library("-mavx2" "avx2" "aom_av1_encoder"
                                "AOM_AV1_ENCODER_AVX2_INTRIN")
endif ()

# Create aom_dsp targets and update dependent targets.
setup_aom_dsp_targets()

# Make all library targets depend on aom_rtcd to make sure it builds first.
foreach (aom_lib ${AOM_LIB_TARGETS})
  if (NOT "${aom_lib}" STREQUAL "aom_rtcd")
    add_dependencies(${aom_lib} aom_rtcd)
  endif ()
endforeach ()
#
# Application and application support targets.
#
add_library(aom_common_app_util OBJECT ${AOM_COMMON_APP_UTIL_SOURCES})
add_library(aom_decoder_app_util OBJECT ${AOM_DECODER_APP_UTIL_SOURCES})
add_library(aom_encoder_app_util OBJECT ${AOM_ENCODER_APP_UTIL_SOURCES})

add_executable(aom_cx_set_ref
               "${AOM_ROOT}/examples/aom_cx_set_ref.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_encoder_app_util>)

add_executable(aomdec
               "${AOM_ROOT}/aomdec.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_decoder_app_util>)

add_executable(aomenc
               "${AOM_ROOT}/aomenc.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_encoder_app_util>
               $<TARGET_OBJECTS:aom_encoder_stats>)

add_executable(decode_to_md5
               "${AOM_ROOT}/examples/decode_to_md5.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_decoder_app_util>)

add_executable(decode_with_drops
               "${AOM_ROOT}/examples/decode_with_drops.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_decoder_app_util>)

add_executable(lossless_encoder
               "${AOM_ROOT}/examples/lossless_encoder.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_encoder_app_util>)

add_executable(set_maps
               "${AOM_ROOT}/examples/set_maps.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_encoder_app_util>)

add_executable(simple_decoder
               "${AOM_ROOT}/examples/simple_decoder.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_decoder_app_util>)

add_executable(simple_encoder
               "${AOM_ROOT}/examples/simple_encoder.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_encoder_app_util>)

add_executable(twopass_encoder
               "${AOM_ROOT}/examples/twopass_encoder.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_encoder_app_util>)

# List of app targets.
set(AOM_APP_TARGETS ${AOM_APP_TARGETS} aom_cx_set_ref aomdec aomenc
    decode_to_md5 decode_with_drops lossless_encoder set_maps simple_decoder
    simple_encoder twopass_encoder)

foreach (aom_app ${AOM_APP_TARGETS})
  target_link_libraries(${aom_app} PUBLIC aom)
endforeach ()

if (CONFIG_LIBYUV)
  add_library(yuv OBJECT ${AOM_LIBYUV_SOURCES})
  if (NOT MSVC)
    target_compile_options(yuv PRIVATE -Wno-unused-parameter)
  endif ()
  include_directories("${AOM_ROOT}/third_party/libyuv/include")

  # Add to existing targets.
  foreach (aom_app ${AOM_APP_TARGETS})
    target_sources(${aom_app} PUBLIC $<TARGET_OBJECTS:yuv>)
    set_property(TARGET ${aom_app} PROPERTY LINKER_LANGUAGE CXX)
  endforeach ()
endif ()

if (CONFIG_WEBM_IO)
  add_library(webm OBJECT ${AOM_LIBWEBM_SOURCES})
  include_directories("${AOM_ROOT}/third_party/libwebm")

  if (NOT MSVC)
    target_compile_options(webm PRIVATE -Wno-shadow)
  endif ()

  # Add to existing targets.
  target_sources(aom_decoder_app_util PUBLIC ${AOM_WEBM_DECODER_SOURCES})
  target_sources(aom_encoder_app_util PUBLIC ${AOM_WEBM_ENCODER_SOURCES})

  foreach (aom_app ${AOM_APP_TARGETS})
    target_sources(${aom_app} PUBLIC $<TARGET_OBJECTS:webm>)
    set_property(TARGET ${aom_app} PROPERTY LINKER_LANGUAGE CXX)
   endforeach ()
endif ()

if (CONFIG_UNIT_TESTS)
  # Create test_libaom target and the targets it depends on.
  setup_aom_test_targets()
endif ()

if (XCODE)
  if (CONFIG_LIBYUV OR CONFIG_WEBM_IO)
    # The Xcode generator does not obey LINKER_LANGUAGE. Because of the issue
    # what looks like a C++ file needs to be in any target that Xcode will link
    # when the target contains a C++ dependency.
    # Without this Xcode will try to link with the C linker, which always ends
    # badly when a dependency actually includes C++.
    # Note: LINKER_LANGUAGE is explicitly set to C++ for all targets touched
    # here, it really is the Xcode generator's fault, or just a deficiency in
    # Xcode itself.
    set(XCODE_DUMMY_CXX_FILE "${AOM_CONFIG_DIR}/dummy.cc")
    file(WRITE "${XCODE_DUMMY_CXX_FILE}"
         "// Xcode needs a C++ file to link, ignore this file.")
    foreach (aom_app ${AOM_APP_TARGETS})
      target_sources(${aom_app} PUBLIC "${XCODE_DUMMY_CXX_FILE}")
    endforeach ()
  endif ()
endif ()
