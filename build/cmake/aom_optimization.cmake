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
if (NOT AOM_BUILD_CMAKE_AOM_OPTIMIZATION_CMAKE_)
set(AOM_BUILD_CMAKE_AOM_OPTIMIZATION_CMAKE_ 1)

# Translate $flag to one which MSVC understands, and write the new flag to the
# variable named by $translated_flag (or unset it, when MSVC needs no flag).
function (get_msvc_intrinsic_flag flag translated_flag)
  if ("${flag}" STREQUAL "-mavx")
    set(${translated_flag} "/arch:AVX" PARENT_SCOPE)
  elseif ("${flag}" STREQUAL "-mavx2")
    set(${translated_flag} "/arch:AVX2" PARENT_SCOPE)
  else ()
    # MSVC does not need flags for intrinsics flavors other than AVX/AVX2.
    unset(${translated_flag} PARENT_SCOPE)
  endif ()
endfunction ()

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
  set(target_name ${target_to_update}_${opt_name}_intrinsics)
  add_library(${target_name} OBJECT ${${sources}})

  if (MSVC)
    get_msvc_intrinsic_flag(${flag} "flag")
  endif ()

  if (flag)
    target_compile_options(${target_name} PUBLIC ${flag})
  endif ()

  target_sources(aom PUBLIC $<TARGET_OBJECTS:${target_name}>)

  # Add the new lib target to the global list of aom library targets.
  list(APPEND AOM_LIB_TARGETS ${target_name})
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} PARENT_SCOPE)
endfunction ()

# Adds sources in list named by $sources to $target and adds $flag to the
# compile flags for each source file.
function (add_intrinsics_source_to_target flag target sources)
  target_sources(${target} PUBLIC ${${sources}})
  if (MSVC)
    get_msvc_intrinsic_flag(${flag} "flag")
  endif ()
  if (flag)
    foreach (source ${${sources}})
      set_property(SOURCE ${source} APPEND PROPERTY COMPILE_FLAGS ${flag})
    endforeach ()
  endif ()
endfunction ()

# Writes object format for the current target to the var named by $out_format,
# or terminates the build when the object format for the current target is
# unknown.
function (get_asm_obj_format out_format)
  if ("${AOM_TARGET_CPU}" STREQUAL "x86_64")
    if ("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
      set(objformat "macho64")
    elseif ("${AOM_TARGET_SYSTEM}" STREQUAL "Linux")
      set(objformat "elf64")
    elseif ("${AOM_TARGET_SYSTEM}" STREQUAL "Windows")
      set(objformat "win64")
    else ()
      message(FATAL_ERROR "Unknown obj format: ${AOM_TARGET_SYSTEM}")
    endif ()
  elseif ("${AOM_TARGET_CPU}" STREQUAL "x86")
    if ("${AOM_TARGET_SYSTEM}" STREQUAL "Darwin")
      set(objformat "macho32")
    elseif ("${AOM_TARGET_SYSTEM}" STREQUAL "Linux")
      set(objformat "elf32")
    elseif ("${AOM_TARGET_SYSTEM}" STREQUAL "Windows")
      set(objformat "win32")
    else ()
      message(FATAL_ERROR "Unknown obj format: ${AOM_TARGET_SYSTEM}")
    endif ()
  else ()
    message(FATAL_ERROR
            "Unknown obj format: ${AOM_TARGET_CPU}-${AOM_TARGET_SYSTEM}")
  endif ()

  set(${out_format} ${objformat} PARENT_SCOPE)
endfunction ()

# Adds library target named $lib_name for ASM files in variable named by
# $asm_sources. Builds an output directory path from $lib_name. Links $lib_name
# into $dependent_target. Generates a dummy C file with a dummy function to
# ensure that all cmake generators can determine the linker language, and that
# build tools don't complain that an object exposes no symbols.
function (add_asm_library lib_name asm_sources dependent_target)
  set(asm_lib_obj_dir "${AOM_CONFIG_DIR}/asm_objects/${lib_name}")
  if (NOT EXISTS "${asm_lib_obj_dir}")
    file(MAKE_DIRECTORY "${asm_lib_obj_dir}")
  endif ()

  # TODO(tomfinegan): If cmake ever allows addition of .o files to OBJECT lib
  # targets, make this OBJECT instead of STATIC to hide the target from
  # consumers of the AOM cmake build.
  add_library(${lib_name} STATIC ${${asm_sources}})

  foreach (asm_source ${${asm_sources}})
    get_filename_component(asm_source_name "${asm_source}" NAME)
    set(asm_object "${asm_lib_obj_dir}/${asm_source_name}.o")
    add_custom_command(OUTPUT "${asm_object}"
                       COMMAND ${AS_EXECUTABLE}
                       ARGS ${AOM_AS_FLAGS}
                            -I${AOM_ROOT} -I${AOM_CONFIG_DIR}
                            -o "${asm_object}" "${asm_source}"
                       DEPENDS "${asm_source}"
                       COMMENT "Building ASM object ${asm_object}"
                       WORKING_DIRECTORY "${AOM_CONFIG_DIR}"
                       VERBATIM)
    target_sources(${lib_name} PRIVATE "${asm_object}")
  endforeach ()

  # The above created a target containing only ASM sources. Cmake needs help
  # here to determine the linker language. Add a dummy C file to force the
  # linker language to C. We don't bother with setting the LINKER_LANGUAGE
  # property on the library target because not all generators obey it (looking
  # at you, xcode generator).
  set(dummy_c_file "${AOM_CONFIG_DIR}/${lib_name}_dummy.c")
  file(WRITE "${dummy_c_file}"
       "// Generated file. DO NOT EDIT!\n"
       "// ${lib_name} needs C file to force link language, ignore me.\n"
       "void ${lib_name}_dummy_function(void) {}\n")
  target_sources(${lib_name} PUBLIC ${dummy_c_file})

  target_link_libraries(${dependent_target} PRIVATE ${lib_name})

  # Add the new lib target to the global list of aom library targets.
  list(APPEND AOM_LIB_TARGETS ${lib_name})
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} PARENT_SCOPE)
endfunction ()

endif ()  # AOM_BUILD_CMAKE_AOM_OPTIMIZATION_CMAKE_
