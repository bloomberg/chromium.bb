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

# Adds an object library target. Terminates generation if $flag is not supported
# by the current compiler. $flag is the intrinsics flag required by the current
# compiler, and is added to the compile flags for all sources in $sources.
# $opt_name is used to name the target. $target_to_update is made
# dependent upon the created target.
#
# Note: the libaom target is always updated because OBJECT libraries have rules
# that disallow the direct addition of .o files to them as dependencies. Static
# libraries do not have this limitation.
function (add_intrinsics_object_library flag opt_name target_to_update sources)
  if (MSVC)
    message(FATAL_ERROR "MSVC instrinics support not implemented.")
  endif ()
  set(target_name ${target_to_update}_${opt_name}_intrinsics)
  add_library(${target_name} OBJECT ${${sources}})
  target_compile_options(${target_name} PUBLIC ${flag})
  target_sources(aom PUBLIC $<TARGET_OBJECTS:${target_name}>)
endfunction ()

# Adds build commands for ASM files in $sources and uses $asm_build_name to
# build an output directory path. Adds ASM objects to libaom directly. $sources
# must be the name of a variable containing a list of sources.
#
# Note: the libaom target is always updated because OBJECT libraries have rules
# that disallow the direct addition of .o files to them as dependencies. Static
# libraries do not have this limitation.
function (add_asm_build asm_build_name sources)
  set(AOM_ASM_OBJECTS_DIR "${AOM_CONFIG_DIR}/asm_objects/${asm_build_name}")
  if (NOT EXISTS "${AOM_ASM_OBJECTS_DIR}")
    file(MAKE_DIRECTORY "${AOM_ASM_OBJECTS_DIR}")
  endif ()

  # TODO(tomfinegan): This might get rather lengthy; probably best to move it
  # out into its own function or macro.
  if ("${AOM_TARGET_CPU}" STREQUAL "x86_64" AND
      "${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
    set(objformat "macho64")
  elseif ("${AOM_TARGET_CPU}" STREQUAL "x86_64" AND
      "${AOM_TARGET_SYSTEM}" STREQUAL "Linux")
    set(objformat "elf64")
  else ()
    message(FATAL_ERROR
            "Unknown obj format: ${AOM_TARGET_CPU}-${AOM_TARGET_SYSTEM}")
  endif ()

  foreach (asm_source ${${sources}})
    get_filename_component(asm_source_name "${asm_source}" NAME)
    set(asm_object "${AOM_ASM_OBJECTS_DIR}/${asm_source_name}.o")
    add_custom_command(OUTPUT "${asm_object}"
                       COMMAND ${YASM_EXECUTABLE}
                       ARGS -f ${objformat}
                            -I${AOM_ROOT} -I${AOM_CONFIG_DIR}
                            -o "${asm_object}" "${asm_source}"
                       DEPENDS "${asm_source}"
                       COMMENT "Building ASM object ${asm_object}"
                       WORKING_DIRECTORY "${AOM_CONFIG_DIR}"
                       VERBATIM)
    target_sources(aom PUBLIC "${asm_source}" "${asm_object}")
  endforeach ()
endfunction ()
