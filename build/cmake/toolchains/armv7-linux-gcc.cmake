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
if (NOT AOM_BUILD_CMAKE_TOOLCHAINS_ARMV7_LINUX_GCC_CMAKE_)
set(AOM_BUILD_CMAKE_TOOLCHAINS_ARMV7_LINUX_GCC_CMAKE_ 1)

set(CMAKE_SYSTEM_NAME "Linux")

# TODO(tomfinegan): Allow control of compiler prefix (aka $CROSS).
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(AS_EXECUTABLE arm-linux-gnueabihf-as)

# TODO(tomfinegan): Handle soft float.
set(CMAKE_C_COMPILER_ARG1 "-march=armv7-a -mfloat-abi=hard -mfpu=neon")
set(CMAKE_CXX_COMPILER_ARG1 "-march=armv7-a -mfloat-abi=hard -mfpu=neon")
set(AOM_AS_FLAGS
    --defsym ARCHITECTURE=7 -march=armv7-a -mfloat-abi=hard -mfpu=neon)

set(CMAKE_SYSTEM_PROCESSOR "armv7")

# No intrinsics flag required for armv7-linux-gcc.
set(AOM_NEON_INTRIN_FLAG "")

# Assembler sources must be converted for armv7-linux-gcc targets.
set(AOM_ADS2GAS_REQUIRED 1)
set(AOM_ADS2GAS "${CMAKE_CURRENT_SOURCE_DIR}/build/make/ads2gas.pl")
set(AOM_GAS_EXT "S")

# No runtime cpu detect for armv7-linux-gcc.
set(CONFIG_RUNTIME_CPU_DETECT 0 CACHE BOOL "")

endif ()  # AOM_BUILD_CMAKE_TOOLCHAINS_ARMV7_LINUX_GCC_CMAKE_
