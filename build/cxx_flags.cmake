##  Copyright (c) 2016 The WebM project authors. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license
##  that can be found in the LICENSE file in the root of the source
##  tree. An additional intellectual property rights grant can be found
##  in the file PATENTS.  All contributing project authors may
##  be found in the AUTHORS file in the root of the source tree.
cmake_minimum_required(VERSION 3.2)

include(CheckCXXCompilerFlag)

# String used to cache failed CXX flags.
set(LIBWEBM_FAILED_CXX_FLAGS)

# Checks C++ compiler for support of $cxx_flag. Adds $cxx_flag to
# $CMAKE_CXX_FLAGS when the compile test passes. Caches $c_flag in
# $LIBWEBM_FAILED_CXX_FLAGS when the test fails.
function (add_cxx_flag_if_supported cxx_flag)
  unset(CXX_FLAG_FOUND CACHE)
  string(FIND "${CMAKE_CXX_FLAGS}" "${cxx_flag}" CXX_FLAG_FOUND)
  unset(CXX_FLAG_FAILED CACHE)
  string(FIND "${LIBWEBM_FAILED_CXX_FLAGS}" "${cxx_flag}" CXX_FLAG_FAILED)

  if (${CXX_FLAG_FOUND} EQUAL -1 AND ${CXX_FLAG_FAILED} EQUAL -1)
    unset(CXX_FLAG_SUPPORTED CACHE)
    message("Checking CXX compiler flag support for: " ${cxx_flag})
    check_cxx_compiler_flag("${cxx_flag}" CXX_FLAG_SUPPORTED)
    if (CXX_FLAG_SUPPORTED)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${cxx_flag}" CACHE STRING ""
          FORCE)
    else ()
      set(LIBWEBM_FAILED_CXX_FLAGS "${LIBWEBM_FAILED_CXX_FLAGS} ${cxx_flag}"
          CACHE STRING "" FORCE)
    endif ()
  endif ()
endfunction ()

if (MSVC)
  add_cxx_flag_if_supported("/W4")
  # Disable MSVC warnings that suggest making code non-portable.
  add_cxx_flag_if_supported("/wd4996")
  if (ENABLE_WERROR)
    add_cxx_flag_if_supported("/WX")
  endif ()
else ()
  add_cxx_flag_if_supported("-Wall")
  add_cxx_flag_if_supported("-Wextra")
  add_cxx_flag_if_supported("-Wnarrowing")
  add_cxx_flag_if_supported("-Wno-deprecated")
  add_cxx_flag_if_supported("-Wshorten-64-to-32")
  if (ENABLE_WERROR)
    add_cxx_flag_if_supported("-Werror")
  endif ()
endif ()
