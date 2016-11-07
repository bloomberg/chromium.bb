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

include("${AOM_ROOT}/build/cmake/aom_config_defaults.cmake")
include("${AOM_ROOT}/build/cmake/compiler_flags.cmake")
include("${AOM_ROOT}/build/cmake/targets/${AOM_TARGET}.cmake")

# TODO(tomfinegan): For some ${AOM_TARGET} values a toolchain can be
# inferred, and we could include it here instead of forcing users to
# remember to explicitly specify ${AOM_TARGET} and the cmake toolchain.

include(FindGit)
include(FindPerl)


# TODO(tomfinegan): consume trailing whitespace after configure_file() when
# target platform check produces empty INLINE and RESTRICT values (aka empty
# values require special casing).
configure_file("${AOM_ROOT}/build/cmake/aom_config.h.cmake"
               "${AOM_CONFIG_DIR}/aom_config.h")

# Read the current git hash.
find_package(Git)
set(AOM_GIT_DESCRIPTION)
set(AOM_GIT_HASH)
if (GIT_FOUND)
  # TODO(tomfinegan): Add build rule so users don't have to re-run cmake to
  # create accurately versioned cmake builds.
  execute_process(COMMAND ${GIT_EXECUTABLE}
                  --git-dir=${AOM_ROOT}/.git rev-parse HEAD
                  OUTPUT_VARIABLE AOM_GIT_HASH)
  execute_process(COMMAND ${GIT_EXECUTABLE} --git-dir=${AOM_ROOT}/.git describe
                  OUTPUT_VARIABLE AOM_GIT_DESCRIPTION ERROR_QUIET)
  # Consume the newline at the end of the git output.
  string(STRIP "${AOM_GIT_HASH}" AOM_GIT_HASH)
  string(STRIP "${AOM_GIT_DESCRIPTION}" AOM_GIT_DESCRIPTION)
endif ()

# TODO(tomfinegan): An alternative to dumping the configure command line to
# aom_config.c is needed in cmake. Normal cmake generation runs do not make the
# command line available in the cmake script. For now, we just set the variable
# to the following. The configure_file() command will expand the message in
# aom_config.c.
# Note: This message isn't strictly true. When cmake is run in script mode (with
# the -P argument), CMAKE_ARGC and CMAKE_ARGVn are defined (n = 0 through
# n = CMAKE_ARGC become valid). Normal cmake generation runs do not make the
# information available.
set(AOM_CMAKE_CONFIG "cmake")
configure_file("${AOM_ROOT}/build/cmake/aom_config.c.cmake"
               "${AOM_CONFIG_DIR}/aom_config.c")

# Find Perl and generate the RTCD sources.
find_package(Perl)
if (NOT PERL_FOUND)
  message(FATAL_ERROR "Perl is required to build libaom.")
endif ()
configure_file(
  "${AOM_ROOT}/build/cmake/targets/rtcd_templates/${AOM_ARCH}.rtcd.cmake"
  "${AOM_CONFIG_DIR}/${AOM_ARCH}.rtcd")

set(AOM_RTCD_CONFIG_FILE_LIST
    "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd_defs.pl"
    "${AOM_ROOT}/aom_scale/aom_scale_rtcd.pl"
    "${AOM_ROOT}/av1/common/av1_rtcd_defs.pl")
set(AOM_RTCD_HEADER_FILE_LIST
    "${AOM_CONFIG_DIR}/aom_dsp_rtcd.h"
    "${AOM_CONFIG_DIR}/aom_scale_rtcd.h"
    "${AOM_CONFIG_DIR}/av1_rtcd.h")
set(AOM_RTCD_SOURCE_FILE_LIST
    "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd.c"
    "${AOM_ROOT}/aom_scale/aom_scale_rtcd.c"
    "${AOM_ROOT}/av1/common/av1_rtcd.c")
set(AOM_RTCD_SYMBOL_LIST aom_dsp_rtcd aom_scale_rtcd aom_av1_rtcd)
list(LENGTH AOM_RTCD_SYMBOL_LIST AOM_RTCD_CUSTOM_COMMAND_COUNT)
math(EXPR AOM_RTCD_CUSTOM_COMMAND_COUNT "${AOM_RTCD_CUSTOM_COMMAND_COUNT} - 1")
foreach(NUM RANGE ${AOM_RTCD_CUSTOM_COMMAND_COUNT})
  list(GET AOM_RTCD_CONFIG_FILE_LIST ${NUM} AOM_RTCD_CONFIG_FILE)
  list(GET AOM_RTCD_HEADER_FILE_LIST ${NUM} AOM_RTCD_HEADER_FILE)
  list(GET AOM_RTCD_SOURCE_FILE_LIST ${NUM} AOM_RTCD_SOURCE_FILE)
  list(GET AOM_RTCD_SYMBOL_LIST ${NUM} AOM_RTCD_SYMBOL)
  execute_process(COMMAND ${PERL_EXECUTABLE} "${AOM_ROOT}/build/make/rtcd.pl"
                  --arch=${AOM_ARCH} --sym=${AOM_RTCD_SYMBOL}
                  --config=${AOM_CONFIG_DIR}/${AOM_ARCH}.rtcd
                  ${AOM_RTCD_CONFIG_FILE}
                  OUTPUT_FILE ${AOM_RTCD_HEADER_FILE})
endforeach()

macro(AomAddRtcdGenerationCommand config output source symbol)
  add_custom_command(OUTPUT ${output}
                     COMMAND ${PERL_EXECUTABLE} "${AOM_ROOT}/build/make/rtcd.pl"
                             --arch=${AOM_ARCH} --sym=${symbol}
                             --config=${AOM_CONFIG_DIR}/${AOM_ARCH}.rtcd
                             ${config} > ${output}
                     DEPENDS ${config}
                     COMMENT "Generating ${output}"
                     WORKING_DIRECTORY ${AOM_CONFIG_DIR}
                     VERBATIM)
  set_property(SOURCE ${source} APPEND PROPERTY OBJECT_DEPENDS ${output})
endmacro()

# Generate aom_version.h.
if ("${AOM_GIT_DESCRIPTION}" STREQUAL "")
  set(AOM_GIT_DESCRIPTION "${AOM_ROOT}/CHANGELOG")
endif ()
execute_process(
  COMMAND ${PERL_EXECUTABLE} "${AOM_ROOT}/build/cmake/aom_version.pl"
  --version_data=${AOM_GIT_DESCRIPTION}
  --version_filename=${AOM_CONFIG_DIR}/aom_version.h)
