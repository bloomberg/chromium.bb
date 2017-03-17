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
if (NOT AOM_BUILD_CMAKE_ARMV7S_IOS_CMAKE_)
set(AOM_BUILD_CMAKE_ARMV7S_IOS_CMAKE_ 1)

if (XCODE)
  # TODO(tomfinegan): Handle arm builds in Xcode.
  message(FATAL_ERROR "This toolchain does not support Xcode.")
endif ()

set(CMAKE_SYSTEM_NAME "Darwin")
set(CMAKE_SYSTEM_PROCESSOR "armv7s")
set(CMAKE_OSX_ARCHITECTURES "armv7s")
set(CMAKE_OSX_SYSROOT iphoneos)
set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_ARG1 "-arch ${CMAKE_SYSTEM_PROCESSOR}")
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_COMPILER_ARG1 "-arch ${CMAKE_SYSTEM_PROCESSOR}")

if (NOT AOM_MIN_IOS_VERSION)
  set(AOM_MIN_IOS_VERSION 9.1)
endif ()
set(CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET ${AOM_MIN_IOS_VERSION})

# No intrinsics flag required for armv7s-ios.
set(AOM_NEON_INTRIN_FLAG "")

# No runtime cpu detect for armv7s-ios.
set(CONFIG_RUNTIME_CPU_DETECT 0 CACHE BOOL "")

# Assembler sources must be converted for ARM iOS targets.
set(AOM_ADS2GAS_REQUIRED 1)
set(AOM_ADS2GAS "${CMAKE_CURRENT_SOURCE_DIR}/build/make/ads2gas_apple.pl")
set(AOM_GAS_EXT "S")

endif ()  # AOM_BUILD_CMAKE_ARMV7S_IOS_CMAKE_
