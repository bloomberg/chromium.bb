##  Copyright (c) 2015 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
cmake_minimum_required(VERSION 2.8)
project(LIBWEBM)
include("${CMAKE_CURRENT_SOURCE_DIR}/build/msvc_runtime.cmake")

set(LIBWEBM_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# Libwebm section.
add_library(webm STATIC
            "${LIBWEBM_SRC_DIR}/mkvmuxer.cpp"
            "${LIBWEBM_SRC_DIR}/mkvmuxer.hpp"
            "${LIBWEBM_SRC_DIR}/mkvmuxertypes.hpp"
            "${LIBWEBM_SRC_DIR}/mkvmuxerutil.cpp"
            "${LIBWEBM_SRC_DIR}/mkvmuxerutil.hpp"
            "${LIBWEBM_SRC_DIR}/mkvparser.cpp"
            "${LIBWEBM_SRC_DIR}/mkvparser.hpp"
            "${LIBWEBM_SRC_DIR}/mkvreader.cpp"
            "${LIBWEBM_SRC_DIR}/mkvreader.hpp"
            "${LIBWEBM_SRC_DIR}/mkvwriter.cpp"
            "${LIBWEBM_SRC_DIR}/mkvwriter.hpp"
            "${LIBWEBM_SRC_DIR}/webmids.hpp")
if(WIN32)
  # Use libwebm and libwebm.lib for project and library name on Windows (instead
  # webm and webm.lib).
  set_target_properties(webm PROPERTIES PROJECT_LABEL libwebm)
  set_target_properties(webm PROPERTIES PREFIX lib)
endif(WIN32)

include_directories("${LIBWEBM_SRC_DIR}")

# Sample section.
add_executable(sample
               "${LIBWEBM_SRC_DIR}/sample.cpp")
target_link_libraries(sample LINK_PUBLIC webm)

# Sample muxer section.
add_executable(sample_muxer
               "${LIBWEBM_SRC_DIR}/sample_muxer.cpp"
               "${LIBWEBM_SRC_DIR}/sample_muxer_metadata.cc"
               "${LIBWEBM_SRC_DIR}/sample_muxer_metadata.h"
               "${LIBWEBM_SRC_DIR}/vttreader.cc"
               "${LIBWEBM_SRC_DIR}/vttreader.h"
               "${LIBWEBM_SRC_DIR}/webvttparser.cc"
               "${LIBWEBM_SRC_DIR}/webvttparser.h")
target_link_libraries(sample_muxer LINK_PUBLIC webm)

# Vttdemux section.
add_executable(vttdemux
               "${LIBWEBM_SRC_DIR}/vttdemux.cc"
               "${LIBWEBM_SRC_DIR}/webvttparser.cc"
               "${LIBWEBM_SRC_DIR}/webvttparser.h")
target_link_libraries(vttdemux LINK_PUBLIC webm)
