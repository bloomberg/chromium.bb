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

include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)

# The basic main() function used in all compile tests.
set(AOM_C_MAIN "\nint main(void) { return 0; }")
set(AOM_CXX_MAIN "\nint main() { return 0; }")

# Strings containing the names of passed and failed tests.
set(AOM_C_PASSED_TESTS)
set(AOM_C_FAILED_TESTS)
set(AOM_CXX_PASSED_TESTS)
set(AOM_CXX_FAILED_TESTS)

# Confirms $test_source compiles and stores $test_name in one of
# $AOM_C_PASSED_TESTS or $AOM_C_FAILED_TESTS depending on out come. When the
# test passes $result_var is set to 1. When it fails $result_var is unset.
# The test is not run if the test name is found in either of the passed or
# failed test variables.
macro(AomCheckCCompiles test_name test_source result_var)
  unset(C_TEST_PASSED CACHE)
  unset(C_TEST_FAILED CACHE)
  string(FIND "${AOM_C_PASSED_TESTS}" "${test_name}" C_TEST_PASSED)
  string(FIND "${AOM_C_FAILED_TESTS}" "${test_name}" C_TEST_FAILED)
  if (${C_TEST_PASSED} EQUAL -1 AND ${C_TEST_FAILED} EQUAL -1)
    unset(C_TEST_COMPILED CACHE)
    message("Running C compiler test: ${test_name}")
    check_c_source_compiles("${test_source} ${AOM_C_MAIN}" C_TEST_COMPILED)
    set(${result_var} ${C_TEST_COMPILED})

    if (C_TEST_COMPILED)
      set(AOM_C_PASSED_TESTS "${AOM_C_PASSED_TESTS} ${test_name}" CACHE STRING
          "" FORCE)
    else ()
      set(AOM_C_FAILED_TESTS "${AOM_C_FAILED_TESTS} ${test_name}" CACHE STRING
          "" FORCE)
      message("C Compiler test ${test_name} failed.")
    endif ()
  elseif (NOT ${C_TEST_PASSED} EQUAL -1)
    set(${result_var} 1)
  else ()  # ${C_TEST_FAILED} NOT EQUAL -1
    unset(${result_var})
  endif ()
endmacro ()

# Confirms $test_source compiles and stores $test_name in one of
# $AOM_CXX_PASSED_TESTS or $AOM_CXX_FAILED_TESTS depending on out come. When the
# test passes $result_var is set to 1. When it fails $result_var is unset.
# The test is not run if the test name is found in either of the passed or
# failed test variables.
macro(AomCheckCxxCompiles test_name test_source result_var)
  unset(CXX_TEST_PASSED CACHE)
  unset(CXX_TEST_FAILED CACHE)
  string(FIND "${AOM_CXX_PASSED_TESTS}" "${test_name}" CXX_TEST_PASSED)
  string(FIND "${AOM_CXX_FAILED_TESTS}" "${test_name}" CXX_TEST_FAILED)
  if (${CXX_TEST_PASSED} EQUAL -1 AND ${CXX_TEST_FAILED} EQUAL -1)
    unset(CXX_TEST_COMPILED CACHE)
    message("Running CXX compiler test: ${test_name}")
    check_cxx_source_compiles("${test_source} ${AOM_CXX_MAIN}"
                              CXX_TEST_COMPILED)
    set(${result_var} ${CXX_TEST_COMPILED})

    if (CXX_TEST_COMPILED)
      set(AOM_CXX_PASSED_TESTS "${AOM_CXX_PASSED_TESTS} ${test_name}" CACHE
          STRING "" FORCE)
    else ()
      set(AOM_CXX_FAILED_TESTS "${AOM_CXX_FAILED_TESTS} ${test_name}" CACHE
          STRING "" FORCE)
      message("CXX Compiler test ${test_name} failed.")
    endif ()
  elseif (NOT ${CXX_TEST_PASSED} EQUAL -1)
    set(${result_var} 1)
  else ()  # ${CXX_TEST_FAILED} NOT EQUAL -1
    unset(${result_var})
  endif ()
endmacro ()

# Convenience macro that confirms $test_source compiles as C and C++.
# $result_var is set to 1 when both tests are successful, and 0 when one or both
# tests fail.
# Note: This macro is intended to be used to write to result variables that are
# expanded via configure_file(). $result_var is set to 1 or 0 to allow direct
# usage of the value in generated source files.
macro(AomCheckSourceCompiles test_name test_source result_var)
  unset(C_PASSED)
  unset(CXX_PASSED)
  AomCheckCCompiles(${test_name} ${test_source} C_PASSED)
  AomCheckCxxCompiles(${test_name} ${test_source} CXX_PASSED)
  if (C_PASSED AND CXX_PASSED)
    set(${result_var} 1)
  else ()
    set(${result_var} 0)
  endif ()
endmacro ()
