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
if (NOT AOM_BUILD_CMAKE_AOM_EXPERIMENT_DEPS_CMAKE_)
set(AOM_BUILD_CMAKE_AOM_EXPERIMENT_DEPS_CMAKE_ 1)

# Adjusts CONFIG_* CMake variables to address conflicts between active AV1
# experiments.
macro (fix_experiment_configs)
  if (CONFIG_AMVR)
    if (NOT CONFIG_HASH_ME)
      change_config_and_warn(CONFIG_HASH_ME 1 CONFIG_AMVR)
    endif ()
  endif ()

  if (CONFIG_SCALABILITY)
    if (NOT CONFIG_OBU)
      change_config_and_warn(CONFIG_OBU 1 CONFIG_SCALABILITY)
    endif ()
    if (NOT CONFIG_OBU_NO_IVF)
      change_config_and_warn(CONFIG_OBU_NO_IVF 1 CONFIG_SCALABILITY)
    endif ()
  endif ()

  if (CONFIG_ANALYZER)
    if (NOT CONFIG_INSPECTION)
      change_config_and_warn(CONFIG_INSPECTION 1 CONFIG_ANALYZER)
    endif ()
  endif ()

  if (CONFIG_EOB_FIRST)
    if (NOT CONFIG_LV_MAP)
      change_config_and_warn(CONFIG_LV_MAP 1 CONFIG_EOB_FIRST)
    endif ()
  endif ()

  if (CONFIG_EXT_INTRA_MOD)
    if (NOT CONFIG_INTRA_EDGE)
      change_config_and_warn(CONFIG_INTRA_EDGE 1 CONFIG_EXT_INTRA_MOD)
    endif ()
  endif ()

  if (CONFIG_EXT_PARTITION_TYPES)
    if (CONFIG_FP_MB_STATS)
      change_config_and_warn(CONFIG_FP_MB_STATS 0 CONFIG_EXT_PARTITION_TYPES)
    endif ()
  endif ()

  if (CONFIG_LOOPFILTER_LEVEL)
    if (NOT CONFIG_EXT_DELTA_Q)
      change_config_and_warn(CONFIG_EXT_DELTA_Q 1 CONFIG_LOOPFILTER_LEVEL)
    endif  ()
  endif ()

  if (CONFIG_TXK_SEL)
    if (NOT CONFIG_LV_MAP)
      change_config_and_warn(CONFIG_LV_MAP 1 CONFIG_TXK_SEL)
    endif ()
  endif ()

  if (CONFIG_AOM_QM_EXT)
    if (NOT CONFIG_AOM_QM)
      change_config_and_warn(CONFIG_AOM_QM 1 CONFIG_AOM_QM_EXT)
    endif ()
  endif ()
endmacro ()

endif ()  # AOM_BUILD_CMAKE_AOM_EXPERIMENT_DEPS_CMAKE_
