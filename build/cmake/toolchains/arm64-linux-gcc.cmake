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
if (NOT AOM_BUILD_CMAKE_TOOLCHAINS_ARM64_LINUX_GCC_CMAKE_)
set(AOM_BUILD_CMAKE_TOOLCHAINS_ARM64_LINUX_GCC_CMAKE_ 1)

set(CMAKE_SYSTEM_NAME "Linux")

# TODO(tomfinegan): Allow control of compiler prefix (aka $CROSS).
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(AS_EXECUTABLE arm-linux-gnueabihf-as)
set(CMAKE_C_COMPILER_ARG1 "-march=armv8-a")
set(CMAKE_CXX_COMPILER_ARG1 "-march=armv8-a")
set(AOM_AS_FLAGS "-march=armv8-a")
set(CMAKE_SYSTEM_PROCESSOR "arm64")

# No intrinsics flag required for arm64-linux-gcc.
set(AOM_NEON_INTRIN_FLAG "")

# No runtime cpu detect for arm64-linux-gcc.
set(CONFIG_RUNTIME_CPU_DETECT 0 CACHE BOOL "")

endif ()  # AOM_BUILD_CMAKE_TOOLCHAINS_ARM64_LINUX_GCC_CMAKE_
