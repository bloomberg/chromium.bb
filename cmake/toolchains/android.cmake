if(LIBGAV1_CMAKE_TOOLCHAINS_ANDROID_CMAKE_)
  return()
endif() # LIBGAV1_CMAKE_TOOLCHAINS_ANDROID_CMAKE_

# Additional ANDROID_* settings are available, see:
# https://developer.android.com/ndk/guides/cmake#variables

if(NOT ANDROID_PLATFORM)
  set(ANDROID_PLATFORM android-21)
endif()

# Choose target architecture with:
#
# -DANDROID_ABI={armeabi-v7a,armeabi-v7a with NEON,arm64-v8a,x86,x86_64}
if(NOT ANDROID_ABI)
  set(ANDROID_ABI arm64-v8a)
endif()

# Force arm mode for 32-bit targets (instead of the default thumb) to improve
# performance.
if(NOT ANDROID_ARM_MODE)
  set(ANDROID_ARM_MODE arm)
endif()

# Toolchain files don't have access to cached variables:
# https://gitlab.kitware.com/cmake/cmake/issues/16170. Set an intermediate
# environment variable when loaded the first time.
if(LIBGAV1_ANDROID_NDK_PATH)
  set(ENV{LIBGAV1_ANDROID_NDK_PATH} "${LIBGAV1_ANDROID_NDK_PATH}")
else()
  set(LIBGAV1_ANDROID_NDK_PATH "$ENV{LIBGAV1_ANDROID_NDK_PATH}")
endif()

if(NOT LIBGAV1_ANDROID_NDK_PATH)
  message(FATAL_ERROR "LIBGAV1_ANDROID_NDK_PATH not set.")
  return()
endif()

include("${LIBGAV1_ANDROID_NDK_PATH}/build/cmake/android.toolchain.cmake")
