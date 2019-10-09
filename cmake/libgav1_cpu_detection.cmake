if(LIBGAV1_CMAKE_LIBGAV1_CPU_DETECTION_CMAKE_)
  return()
endif() # LIBGAV1_CMAKE_LIBGAV1_CPU_DETECTION_CMAKE_
set(LIBGAV1_CMAKE_LIBGAV1_CPU_DETECTION_CMAKE_ 1)

# Detect optimizations available for the current target CPU.
macro(libgav1_optimization_detect)
  if(LIBGAV1_ENABLE_OPTIMIZATIONS)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" cpu_lowercase)
    if(cpu_lowercase MATCHES "^arm|^aarch64")
      set(libgav1_have_neon ON)
    elseif(cpu_lowercase MATCHES "^x86|amd64")
      set(libgav1_have_sse4 ON)
    endif()
  endif()

  if(libgav1_have_neon AND LIBGAV1_ENABLE_NEON)
    list(APPEND libgav1_defines "LIBGAV1_ENABLE_NEON=1")
  else()
    list(APPEND libgav1_defines "LIBGAV1_ENABLE_NEON=0")
  endif()

  if(libgav1_have_sse4 AND LIBGAV1_ENABLE_SSE4_1)
    list(APPEND libgav1_defines "LIBGAV1_ENABLE_SSE4_1=1")
  else()
    list(APPEND libgav1_defines "LIBGAV1_ENABLE_SSE4_1=0")
  endif()
endmacro()
