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
set(AOM_ARCH "generic")

# TODO(tomfinegan): These must not be hard coded. They must be set based on
# the values of the externally configurable vars that omit the RTCD_ prefix.
set(RTCD_CONFIG_AV1 "yes")
set(RTCD_CONFIG_AV1_DECODER "yes")
set(RTCD_CONFIG_AV1_ENCODER "yes")
set(RTCD_CONFIG_INSTALL_BINS "yes")
set(RTCD_CONFIG_INSTALL_LIBS "yes")
set(RTCD_CONFIG_GCC "yes")
set(RTCD_CONFIG_MULTITHREAD "yes")
set(RTCD_CONFIG_AV1_ENCODER "yes")
set(RTCD_CONFIG_AV1_DECODER "yes")
set(RTCD_CONFIG_AV1 "yes")
set(RTCD_CONFIG_ENCODERS "yes")
set(RTCD_CONFIG_DECODERS "yes")
set(RTCD_CONFIG_SPATIAL_RESAMPLING "yes")
set(RTCD_CONFIG_STATIC "yes")
set(RTCD_CONFIG_OS_SUPPORT "yes")
set(RTCD_CONFIG_TEMPORAL_DENOISING "yes")
set(RTCD_CONFIG_RUNTIME_CPU_DETECT "yes")

if (CONFIG_AOM_HIGHBITDEPTH)
  set(RTCD_CONFIG_AOM_HIGHBITDEPTH "yes")
endif ()
if (CONFIG_CDEF)
  set(RTCD_CONFIG_CDEF "yes")
endif ()
if (CONFIG_EXT_INTER)
  set(RTCD_CONFIG_EXT_INTER "yes")
endif ()
if (CONFIG_INTERNAL_STATS)
  set(RTCD_CONFIG_INTERNAL_STATS "yes")
endif ()
