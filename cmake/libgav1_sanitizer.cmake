if(LIBGAV1_CMAKE_LIBGAV1_SANITIZER_CMAKE_)
  return()
endif() # LIBGAV1_CMAKE_LIBGAV1_SANITIZER_CMAKE_
set(LIBGAV1_CMAKE_LIBGAV1_SANITIZER_CMAKE_ 1)

macro(libgav1_configure_sanitizer)
  if(LIBGAV1_SANITIZE AND NOT MSVC)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      if(LIBGAV1_SANITIZE MATCHES "cfi")
        list(APPEND LIBGAV1_CXX_FLAGS "-flto" "-fno-sanitize-trap=cfi")
        list(APPEND LIBGAV1_EXE_LINKER_FLAGS "-flto" "-fno-sanitize-trap=cfi"
                    "-fuse-ld=gold")
      endif()

      if(${CMAKE_SIZEOF_VOID_P} EQUAL 4
         AND LIBGAV1_SANITIZE MATCHES "integer|undefined")
        list(APPEND LIBGAV1_EXE_LINKER_FLAGS "--rtlib=compiler-rt" "-lgcc_s")
      endif()
    endif()

    list(APPEND LIBGAV1_CXX_FLAGS "-fsanitize=${LIBGAV1_SANITIZE}")
    list(APPEND LIBGAV1_EXE_LINKER_FLAGS "-fsanitize=${LIBGAV1_SANITIZE}")

    # Make sanitizer callstacks accurate.
    list(APPEND LIBGAV1_CXX_FLAGS "-fno-omit-frame-pointer"
                "-fno-optimize-sibling-calls")

    libgav1_test_cxx_flag(FLAG_LIST_VAR_NAMES LIBGAV1_CXX_FLAGS FLAG_REQUIRED)
    libgav1_test_exe_linker_flag(FLAG_LIST_VAR_NAME LIBGAV1_EXE_LINKER_FLAGS)
  endif()
endmacro()
