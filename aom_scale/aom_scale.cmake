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
set(AOM_SCALE_SOURCES
    "${AOM_ROOT}/aom_scale/aom_scale.h"
    "${AOM_ROOT}/aom_scale/generic/aom_scale.c"
    "${AOM_ROOT}/aom_scale/generic/gen_scalers.c"
    "${AOM_ROOT}/aom_scale/generic/yv12config.c"
    "${AOM_ROOT}/aom_scale/generic/yv12extend.c"
    "${AOM_ROOT}/aom_scale/yv12config.h")

# Creates the aom_scale build target and makes libaom depend on it. The libaom
# target must exist before this function is called.
function (setup_aom_scale_targets)
  add_library(aom_scale OBJECT ${AOM_SCALE_SOURCES})
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} aom_scale PARENT_SCOPE)
  target_sources(aom PUBLIC $<TARGET_OBJECTS:aom_scale>)
endfunction ()
