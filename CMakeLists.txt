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

# Enable C++11 support.
set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)

# Turn up the warning levels.
if(MSVC)
  set(CMAKE_CXX_FLAGS "/W4 ${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)
else ()
  set(CMAKE_CXX_FLAGS "-Wall -Wextra ${CMAKE_CXX_FLAGS}" CACHE STRING "" FORCE)
endif()

# Test configuration flags. Defined here for visibility.
option(ENABLE_TESTS "Enables tests." OFF)
set(GTEST_SRC_DIR "${LIBWEBM_SRC_DIR}/../googletest" CACHE PATH
    "Path to Googletest git repository.")

# This directory is where libwebm will build googletest dependencies.
set(GTEST_BUILD_DIR "${CMAKE_BINARY_DIR}/googletest_build")

# Libwebm section.
add_library(webm STATIC
            "${LIBWEBM_SRC_DIR}/hdr_util.cpp"
            "${LIBWEBM_SRC_DIR}/hdr_util.hpp"
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

# Sample (mkvparser) section.
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

# webmts (PES/TS support) library section.
add_library(webmts
            "${LIBWEBM_SRC_DIR}/common/libwebm_utils.cc"
            "${LIBWEBM_SRC_DIR}/common/libwebm_utils.h"
            "${LIBWEBM_SRC_DIR}/webm2pes.cc"
            "${LIBWEBM_SRC_DIR}/webm2pes.h"
            "${LIBWEBM_SRC_DIR}/vpxpes2ts.cc"
            "${LIBWEBM_SRC_DIR}/vpxpes2ts.h")

# webm2pes section.
add_executable(webm2pes
               "${LIBWEBM_SRC_DIR}/webm2pes_main.cc")
target_link_libraries(webm2pes LINK_PUBLIC webm webmts)

# webm2ts section.
add_executable(webm2ts "${LIBWEBM_SRC_DIR}/vpxpes2ts_main.cc")
target_link_libraries(webm2ts LINK_PUBLIC webm webmts)

if (ENABLE_TESTS)
  add_subdirectory("${GTEST_SRC_DIR}" "${GTEST_BUILD_DIR}")
  include_directories("${GTEST_SRC_DIR}/googletest/include")

  add_executable(muxer_tests
                 "${LIBWEBM_SRC_DIR}/testing/muxer_tests.cc"
                 "${LIBWEBM_SRC_DIR}/testing/test_util.cc"
                 "${LIBWEBM_SRC_DIR}/testing/test_util.h")
  target_link_libraries(muxer_tests LINK_PUBLIC gtest webm)

  add_executable(parser_tests
                 "${LIBWEBM_SRC_DIR}/testing/parser_tests.cc"
                 "${LIBWEBM_SRC_DIR}/testing/test_util.cc"
                 "${LIBWEBM_SRC_DIR}/testing/test_util.h")
  target_link_libraries(parser_tests LINK_PUBLIC gtest webm)

  add_executable(webm2pes_tests
                 "${LIBWEBM_SRC_DIR}/testing/test_util.cc"
                 "${LIBWEBM_SRC_DIR}/testing/test_util.h"
                 "${LIBWEBM_SRC_DIR}/testing/webm2pes_tests.cc")
  target_link_libraries(webm2pes_tests LINK_PUBLIC gtest webm webmts)
endif (ENABLE_TESTS)

