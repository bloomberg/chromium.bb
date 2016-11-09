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

# TODO(tomfinegan): Host auto detect goes here when $AOM_TARGET is empty.
set(AOM_TARGET "generic")
include("${AOM_ROOT}/build/cmake/aom_configure.cmake")

# TODO(tomfinegan): Use libwebm's cmake support directly.
set(AOM_LIBWEBM_SRCS
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

set(AOM_LIBYUV_SRCS
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

set(AOM_SRCS
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

set(AOM_DSP_SRCS
    "${AOM_ROOT}/aom_dsp/aom_convolve.c"
    "${AOM_ROOT}/aom_dsp/aom_convolve.h"
    "${AOM_ROOT}/aom_dsp/aom_dsp_common.h"
    "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd.c"
    "${AOM_CONFIG_DIR}/aom_dsp_rtcd.h"
    "${AOM_ROOT}/aom_dsp/aom_filter.h"
    "${AOM_ROOT}/aom_dsp/aom_simd.c"
    "${AOM_ROOT}/aom_dsp/aom_simd.h"
    "${AOM_ROOT}/aom_dsp/aom_simd_inline.h"
    "${AOM_ROOT}/aom_dsp/avg.c"
    "${AOM_ROOT}/aom_dsp/bitreader.h"
    "${AOM_ROOT}/aom_dsp/bitreader_buffer.c"
    "${AOM_ROOT}/aom_dsp/bitreader_buffer.h"
    "${AOM_ROOT}/aom_dsp/bitwriter.h"
    "${AOM_ROOT}/aom_dsp/bitwriter_buffer.c"
    "${AOM_ROOT}/aom_dsp/bitwriter_buffer.h"
    "${AOM_ROOT}/aom_dsp/blend.h"
    "${AOM_ROOT}/aom_dsp/blend_a64_hmask.c"
    "${AOM_ROOT}/aom_dsp/blend_a64_mask.c"
    "${AOM_ROOT}/aom_dsp/blend_a64_vmask.c"
    "${AOM_ROOT}/aom_dsp/daalaboolreader.c"
    "${AOM_ROOT}/aom_dsp/daalaboolreader.h"
    "${AOM_ROOT}/aom_dsp/daalaboolwriter.c"
    "${AOM_ROOT}/aom_dsp/daalaboolwriter.h"
    "${AOM_ROOT}/aom_dsp/dkboolreader.c"
    "${AOM_ROOT}/aom_dsp/dkboolreader.h"
    "${AOM_ROOT}/aom_dsp/dkboolwriter.c"
    "${AOM_ROOT}/aom_dsp/dkboolwriter.h"
    "${AOM_ROOT}/aom_dsp/entcode.c"
    "${AOM_ROOT}/aom_dsp/entcode.h"
    "${AOM_ROOT}/aom_dsp/entdec.c"
    "${AOM_ROOT}/aom_dsp/entdec.h"
    "${AOM_ROOT}/aom_dsp/entenc.c"
    "${AOM_ROOT}/aom_dsp/entenc.h"
    "${AOM_ROOT}/aom_dsp/fwd_txfm.c"
    "${AOM_ROOT}/aom_dsp/fwd_txfm.h"
    "${AOM_ROOT}/aom_dsp/intrapred.c"
    "${AOM_ROOT}/aom_dsp/inv_txfm.c"
    "${AOM_ROOT}/aom_dsp/inv_txfm.h"
    "${AOM_ROOT}/aom_dsp/loopfilter.c"
    "${AOM_ROOT}/aom_dsp/prob.c"
    "${AOM_ROOT}/aom_dsp/prob.h"
    "${AOM_ROOT}/aom_dsp/psnr.c"
    "${AOM_ROOT}/aom_dsp/psnr.h"
    "${AOM_ROOT}/aom_dsp/quantize.c"
    "${AOM_ROOT}/aom_dsp/quantize.h"
    "${AOM_ROOT}/aom_dsp/sad.c"
    "${AOM_ROOT}/aom_dsp/simd/v128_intrinsics.h"
    "${AOM_ROOT}/aom_dsp/simd/v128_intrinsics_c.h"
    "${AOM_ROOT}/aom_dsp/simd/v256_intrinsics.h"
    "${AOM_ROOT}/aom_dsp/simd/v256_intrinsics_c.h"
    "${AOM_ROOT}/aom_dsp/simd/v64_intrinsics.h"
    "${AOM_ROOT}/aom_dsp/simd/v64_intrinsics_c.h"
    "${AOM_ROOT}/aom_dsp/subtract.c"
    "${AOM_ROOT}/aom_dsp/sum_squares.c"
    "${AOM_ROOT}/aom_dsp/txfm_common.h"
    "${AOM_ROOT}/aom_dsp/variance.c"
    "${AOM_ROOT}/aom_dsp/variance.h"
    "${AOM_ROOT}/aom_dsp/x86/txfm_common_intrin.h")

set(AOM_MEM_SRCS
    "${AOM_ROOT}/aom_mem/aom_mem.c"
    "${AOM_ROOT}/aom_mem/aom_mem.h"
    "${AOM_ROOT}/aom_mem/include/aom_mem_intrnl.h")

set(AOM_SCALE_SRCS
    "${AOM_ROOT}/aom_scale/aom_scale.h"
    "${AOM_ROOT}/aom_scale/aom_scale_rtcd.c"
    "${AOM_CONFIG_DIR}/aom_scale_rtcd.h"
    "${AOM_ROOT}/aom_scale/generic/aom_scale.c"
    "${AOM_ROOT}/aom_scale/generic/gen_scalers.c"
    "${AOM_ROOT}/aom_scale/generic/yv12config.c"
    "${AOM_ROOT}/aom_scale/generic/yv12extend.c"
    "${AOM_ROOT}/aom_scale/yv12config.h")

# TODO(tomfinegan): Extract aom_ports from aom_util if possible.
# TODO(tomfinegan): aom_ports has some target specific source files.
# aom_configure will need to set a var that gets picked up here.
set(AOM_UTIL_SRCS
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

set(AOM_AV1_COMMON_SRCS
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
    "${AOM_ROOT}/av1/common/av1_rtcd.c"
    "${AOM_CONFIG_DIR}/av1_rtcd.h"
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
    "${AOM_ROOT}/av1/common/loopfilter.c"
    "${AOM_ROOT}/av1/common/loopfilter.h"
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

set(AOM_AV1_DECODER_SRCS
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

set(AOM_AV1_ENCODER_SRCS
    "${AOM_ROOT}/av1/av1_cx_iface.c"
    "${AOM_ROOT}/av1/encoder/aq_complexity.c"
    "${AOM_ROOT}/av1/encoder/aq_complexity.h"
    "${AOM_ROOT}/av1/encoder/aq_cyclicrefresh.c"
    "${AOM_ROOT}/av1/encoder/aq_cyclicrefresh.h"
    "${AOM_ROOT}/av1/encoder/aq_variance.c"
    "${AOM_ROOT}/av1/encoder/aq_variance.h"
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
    "${AOM_ROOT}/av1/encoder/quantize.c"
    "${AOM_ROOT}/av1/encoder/quantize.h"
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
    "${AOM_ROOT}/md5_utils.c"
    "${AOM_ROOT}/md5_utils.h"
    "${AOM_ROOT}/tools_common.c"
    "${AOM_ROOT}/tools_common.h"
    "${AOM_ROOT}/video_common.h")

set(AOM_DECODER_APP_UTIL_SOURCES
    "${AOM_ROOT}/ivfdec.c"
    "${AOM_ROOT}/ivfdec.h"
    "${AOM_ROOT}/video_reader.c"
    "${AOM_ROOT}/video_reader.h")

set(AOM_ENCODER_APP_UTIL_SOURCES
    "${AOM_ROOT}/ivfenc.c"
    "${AOM_ROOT}/ivfenc.h"
    "${AOM_ROOT}/video_writer.c"
    "${AOM_ROOT}/video_writer.h")

# Targets
# TODO(tomfinegan): Add support for enable/disable encoder/decoder.
# TODO(tomfinegan): Add support for WEBM_IO from the old build system.
add_library(aom_dsp OBJECT ${AOM_DSP_SRCS})
include_directories(${AOM_ROOT} ${AOM_CONFIG_DIR})
AomAddRtcdGenerationCommand(${AOM_ROOT}/aom_dsp/aom_dsp_rtcd_defs.pl
                            ${AOM_CONFIG_DIR}/aom_dsp_rtcd.h
                            ${AOM_CONFIG_DIR}/aom_dsp/aom_dsp_rtcd.c
                            aom_dsp_rtcd)

add_library(aom_mem OBJECT ${AOM_MEM_SRCS})

add_library(aom_scale OBJECT ${AOM_SCALE_SRCS})
include_directories(${AOM_ROOT} ${AOM_CONFIG_DIR})
AomAddRtcdGenerationCommand(${AOM_ROOT}/aom_scale/aom_scale_rtcd.pl
                            ${AOM_CONFIG_DIR}/aom_scale_rtcd.h
                            ${AOM_CONFIG_DIR}/aom_scale/aom_scale_rtcd.c
                            aom_scale_rtcd)

add_library(aom_util OBJECT ${AOM_UTIL_SRCS})

add_library(aom_av1_common OBJECT ${AOM_AV1_COMMON_SRCS})
include_directories(${AOM_ROOT} ${AOM_CONFIG_DIR})
AomAddRtcdGenerationCommand(${AOM_ROOT}/av1/common/av1_rtcd_defs.pl
                            ${AOM_CONFIG_DIR}/av1_rtcd.h
                            ${AOM_CONFIG_DIR}/av1/common/av1_rtcd.c
                            aom_av1_rtcd)

add_library(aom_av1_decoder OBJECT ${AOM_AV1_DECODER_SRCS})
add_library(aom_av1_encoder OBJECT ${AOM_AV1_ENCODER_SRCS})
add_library(aom
            ${AOM_SRCS}
            $<TARGET_OBJECTS:aom_dsp>
            $<TARGET_OBJECTS:aom_mem>
            $<TARGET_OBJECTS:aom_scale>
            $<TARGET_OBJECTS:aom_util>
            $<TARGET_OBJECTS:aom_av1_common>
            $<TARGET_OBJECTS:aom_av1_decoder>
            $<TARGET_OBJECTS:aom_av1_encoder>)

add_library(aom_common_app_util OBJECT ${AOM_COMMON_APP_UTIL_SOURCES})
add_library(aom_decoder_app_util OBJECT ${AOM_DECODER_APP_UTIL_SOURCES})
add_library(aom_encoder_app_util OBJECT ${AOM_ENCODER_APP_UTIL_SOURCES})

add_executable(aom_cx_set_ref
               "${AOM_ROOT}/examples/aom_cx_set_ref.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_encoder_app_util>)
include_directories(${AOM_ROOT})
target_link_libraries(aom_cx_set_ref PUBLIC aom)

add_executable(decode_to_md5
               "${AOM_ROOT}/examples/decode_to_md5.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_decoder_app_util>)
include_directories(${AOM_ROOT})
target_link_libraries(decode_to_md5 PUBLIC aom)

add_executable(decode_with_drops
               "${AOM_ROOT}/examples/decode_with_drops.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_decoder_app_util>)
include_directories(${AOM_ROOT})
target_link_libraries(decode_with_drops PUBLIC aom)

add_executable(lossless_encoder
               "${AOM_ROOT}/examples/lossless_encoder.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_encoder_app_util>)
include_directories(${AOM_ROOT})
target_link_libraries(lossless_encoder PUBLIC aom)

add_executable(set_maps
               "${AOM_ROOT}/examples/set_maps.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_encoder_app_util>)
include_directories(${AOM_ROOT})
target_link_libraries(set_maps PUBLIC aom)

add_executable(simple_decoder
               "${AOM_ROOT}/examples/simple_decoder.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_decoder_app_util>)
include_directories(${AOM_ROOT})
target_link_libraries(simple_decoder PUBLIC aom)

add_executable(simple_encoder
               "${AOM_ROOT}/examples/simple_encoder.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_encoder_app_util>)
include_directories(${AOM_ROOT})
target_link_libraries(simple_encoder PUBLIC aom)

add_executable(twopass_encoder
               "${AOM_ROOT}/examples/twopass_encoder.c"
               $<TARGET_OBJECTS:aom_common_app_util>
               $<TARGET_OBJECTS:aom_encoder_app_util>)
include_directories(${AOM_ROOT})
target_link_libraries(twopass_encoder PUBLIC aom)

if (CONFIG_LIBYUV)
  add_library(yuv ${AOM_LIBYUV_SRCS})
  target_compile_options(yuv PRIVATE -Wno-unused-parameter)
  include_directories("${AOM_ROOT}/third_party/libyuv/include")

  # Add to existing targets.
  target_link_libraries(aom_cx_set_ref PUBLIC yuv)
  target_link_libraries(decode_to_md5 PUBLIC yuv)
  target_link_libraries(decode_with_drops PUBLIC yuv)
  target_link_libraries(lossless_encoder PUBLIC yuv)
  target_link_libraries(set_maps PUBLIC yuv)
  target_link_libraries(simple_decoder PUBLIC yuv)
  target_link_libraries(simple_encoder PUBLIC yuv)
  target_link_libraries(twopass_encoder PUBLIC yuv)
endif ()

if (CONFIG_WEBM_IO)
  add_library(webm ${AOM_LIBWEBM_SRCS})
  include_directories("${AOM_ROOT}/third_party/libwebm")
  target_compile_options(webm PRIVATE -Wno-shadow)

  # Add to existing targets.
  target_link_libraries(aom_cx_set_ref PUBLIC webm)
  target_link_libraries(decode_to_md5 PUBLIC webm)
  target_link_libraries(decode_with_drops PUBLIC webm)
  target_link_libraries(lossless_encoder PUBLIC webm)
  target_link_libraries(set_maps PUBLIC webm)
  target_link_libraries(simple_decoder PUBLIC webm)
  target_link_libraries(simple_encoder PUBLIC webm)
  target_link_libraries(twopass_encoder PUBLIC webm)
endif ()
