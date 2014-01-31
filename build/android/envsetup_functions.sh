#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Defines functions for envsetup.sh which sets up environment for building
# Chromium on Android.  The build can be either use the Android NDK/SDK or
# android source tree.  Each has a unique init function which calls functions
# prefixed with "common_" that is common for both environment setups.

################################################################################
# Check to make sure the toolchain exists for the NDK version.
################################################################################
common_check_toolchain() {
  if [[ ! -d "${ANDROID_TOOLCHAIN}" ]]; then
    echo "Can not find Android toolchain in ${ANDROID_TOOLCHAIN}." >& 2
    echo "The NDK version might be wrong." >& 2
    return 1
  fi
}

################################################################################
# Exports environment variables common to both sdk and non-sdk build (e.g. PATH)
# based on CHROME_SRC and ANDROID_TOOLCHAIN, along with DEFINES for GYP_DEFINES.
################################################################################
common_vars_defines() {
  # Set toolchain path according to product architecture.
  case "${TARGET_ARCH}" in
    "arm")
      toolchain_arch="arm-linux-androideabi"
      ;;
    "x86")
      toolchain_arch="x86"
      ;;
    "mips")
      toolchain_arch="mipsel-linux-android"
      ;;
    *)
      echo "TARGET_ARCH: ${TARGET_ARCH} is not supported." >& 2
      print_usage
      return 1
      ;;
  esac

  toolchain_version="4.6"
  toolchain_target=$(basename \
    ${ANDROID_NDK_ROOT}/toolchains/${toolchain_arch}-${toolchain_version})
  toolchain_path="${ANDROID_NDK_ROOT}/toolchains/${toolchain_target}"\
"/prebuilt/${toolchain_dir}/bin/"

  # Set only if not already set.
  # Don't override ANDROID_TOOLCHAIN if set by Android configuration env.
  export ANDROID_TOOLCHAIN=${ANDROID_TOOLCHAIN:-${toolchain_path}}

  common_check_toolchain

  # Add Android SDK/NDK tools to system path.
  export PATH=$PATH:${ANDROID_NDK_ROOT}
  export PATH=$PATH:${ANDROID_SDK_ROOT}/tools
  export PATH=$PATH:${ANDROID_SDK_ROOT}/platform-tools
  export PATH=$PATH:${ANDROID_SDK_ROOT}/build-tools/\
${ANDROID_SDK_BUILD_TOOLS_VERSION}

  # Add Chromium Android development scripts to system path.
  # Must be after CHROME_SRC is set.
  export PATH=$PATH:${CHROME_SRC}/build/android

  # TODO(beverloo): Remove these once all consumers updated to --strip-binary.
  # http://crbug.com/142642
  export STRIP=$(echo ${ANDROID_TOOLCHAIN}/*-strip)

  # The set of GYP_DEFINES to pass to gyp.
  DEFINES="OS=android"
  DEFINES+=" host_os=${host_os}"

  if [[ -n "$CHROME_ANDROID_OFFICIAL_BUILD" ]]; then
    DEFINES+=" branding=Chrome"
    DEFINES+=" buildtype=Official"

    # These defines are used by various chrome build scripts to tag the binary's
    # version string as 'official' in linux builds (e.g. in
    # chrome/trunk/src/chrome/tools/build/version.py).
    export OFFICIAL_BUILD=1
    export CHROMIUM_BUILD="_google_chrome"
    export CHROME_BUILD_TYPE="_official"
  fi

  # TODO(thakis), Jan 18 2014: Remove this after two weeks or so, after telling
  # everyone to set use_goma in GYP_DEFINES instead of a GOMA_DIR env var.
  if [[ -d $GOMA_DIR ]]; then
    DEFINES+=" use_goma=1 gomadir=$GOMA_DIR"
  fi

  # The following defines will affect ARM code generation of both C/C++ compiler
  # and V8 mksnapshot.
  case "${TARGET_ARCH}" in
    "arm")
      DEFINES+=" target_arch=arm"
      ;;
    "x86")
      host_arch=$(uname -m | sed -e \
        's/i.86/ia32/;s/x86_64/x64/;s/amd64/x64/;s/arm.*/arm/;s/i86pc/ia32/')
      DEFINES+=" host_arch=${host_arch}"
      DEFINES+=" target_arch=ia32"
      ;;
    "mips")
      DEFINES+=" target_arch=mipsel"
      DEFINES+=" mips_arch_variant=mips32r1"
      ;;
    *)
      echo "TARGET_ARCH: ${TARGET_ARCH} is not supported." >& 2
      print_usage
      return 1
  esac
}


################################################################################
# Exports common GYP variables based on variable DEFINES and CHROME_SRC.
################################################################################
common_gyp_vars() {
  export GYP_DEFINES="${DEFINES}"

  # TODO(thakis): Remove this after a week or two. Sourcing envsetup.sh used to
  # set this variable, but now that all_android.gyp is gone having it set will
  # lead to errors, so explicitly unset it to remove it from the environment of
  # developers who keep their shells open for weeks (most of them, probably).
  unset CHROMIUM_GYP_FILE
}


################################################################################
# Prints out help message on usage.
################################################################################
print_usage() {
  echo "usage: ${0##*/} [--target-arch=value] [--help]" >& 2
  echo "--target-arch=value     target CPU architecture (arm=default, x86)" >& 2
  echo "--host-os=value         override host OS detection (linux, mac)" >&2
  echo "--try-32bit-host        try building a 32-bit host architecture" >&2
  echo "--help                  this help" >& 2
}

################################################################################
# Process command line options.
################################################################################
process_options() {
  host_os=$(uname -s | sed -e 's/Linux/linux/;s/Darwin/mac/')
  try_32bit_host_build=
  while [[ -n $1 ]]; do
    case "$1" in
      --target-arch=*)
        target_arch="$(echo "$1" | sed 's/^[^=]*=//')"
        ;;
      --host-os=*)
        host_os="$(echo "$1" | sed 's/^[^=]*=//')"
        ;;
      --try-32bit-host)
        try_32bit_host_build=true
        ;;
      --help)
        print_usage
        return 1
        ;;
      *)
        # Ignore other command line options
        echo "Unknown option: $1"
        ;;
    esac
    shift
  done

  # Sets TARGET_ARCH. Defaults to arm if not specified.
  TARGET_ARCH=${target_arch:-arm}
}

################################################################################
# Initializes environment variables for NDK/SDK build.
################################################################################
sdk_build_init() {

  # Allow the caller to override a few environment variables. If any of them is
  # unset, we default to a sane value that's known to work. This allows for
  # experimentation with a custom SDK.
  local sdk_defines=""
  if [[ -z "${ANDROID_NDK_ROOT}" || ! -d "${ANDROID_NDK_ROOT}" ]]; then
    export ANDROID_NDK_ROOT="${CHROME_SRC}/third_party/android_tools/ndk/"
  fi
  if [[ -z "${ANDROID_SDK_VERSION}" ]]; then
    export ANDROID_SDK_VERSION=19
  else
    sdk_defines+=" android_sdk_version=${ANDROID_SDK_VERSION}"
  fi
  local sdk_suffix=platforms/android-${ANDROID_SDK_VERSION}
  if [[ -z "${ANDROID_SDK_ROOT}" || \
       ! -d "${ANDROID_SDK_ROOT}/${sdk_suffix}" ]]; then
    export ANDROID_SDK_ROOT="${CHROME_SRC}/third_party/android_tools/sdk/"
  else
    sdk_defines+=" android_sdk_root=${ANDROID_SDK_ROOT}"
  fi
  if [[ -z "${ANDROID_SDK_BUILD_TOOLS_VERSION}" ]]; then
    export ANDROID_SDK_BUILD_TOOLS_VERSION=19.0.0
  fi

  # Unset toolchain. This makes it easy to switch between architectures.
  unset ANDROID_TOOLCHAIN

  common_vars_defines

  DEFINES+="${sdk_defines}"

  common_gyp_vars

  if [[ -n "$CHROME_ANDROID_BUILD_WEBVIEW" ]]; then
    # Can not build WebView with NDK/SDK because it needs the Android build
    # system and build inside an Android source tree.
    echo "Can not build WebView with NDK/SDK.  Requires android source tree." \
        >& 2
    echo "Try . build/android/envsetup.sh instead." >& 2
    return 1
  fi

  # Directory containing build-tools: aapt, aidl, dx
  export ANDROID_SDK_TOOLS="${ANDROID_SDK_ROOT}/build-tools/\
${ANDROID_SDK_BUILD_TOOLS_VERSION}"
}

################################################################################
# To build WebView, we use the Android build system and build inside an Android
# source tree.
#############################################################################
webview_build_init() {
  # Use the latest API in the AOSP prebuilts directory (change with AOSP roll).
  export ANDROID_SDK_VERSION=18

  # For the WebView build we always use the NDK and SDK in the Android tree,
  # and we don't touch ANDROID_TOOLCHAIN which is already set by Android.
  export ANDROID_NDK_ROOT=${ANDROID_BUILD_TOP}/prebuilts/ndk/8
  export ANDROID_SDK_ROOT=${ANDROID_BUILD_TOP}/prebuilts/sdk/\
${ANDROID_SDK_VERSION}

  common_vars_defines

  # We need to supply SDK paths relative to the top of the Android tree to make
  # sure the generated Android makefiles are portable, as they will be checked
  # into the Android tree.
  ANDROID_SDK=$(python -c \
      "import os.path; print os.path.relpath('${ANDROID_SDK_ROOT}', \
      '${ANDROID_BUILD_TOP}')")
  case "${host_os}" in
    "linux")
      ANDROID_SDK_TOOLS=$(python -c \
          "import os.path; \
          print os.path.relpath('${ANDROID_SDK_ROOT}/../tools/linux', \
          '${ANDROID_BUILD_TOP}')")
      ;;
    "mac")
      ANDROID_SDK_TOOLS=$(python -c \
          "import os.path; \
          print os.path.relpath('${ANDROID_SDK_ROOT}/../tools/darwin', \
          '${ANDROID_BUILD_TOP}')")
      ;;
  esac
  DEFINES+=" android_webview_build=1"
  DEFINES+=" android_src=\$(PWD)"
  DEFINES+=" android_sdk=\$(PWD)/${ANDROID_SDK}"
  DEFINES+=" android_sdk_root=\$(PWD)/${ANDROID_SDK}"
  DEFINES+=" android_sdk_tools=\$(PWD)/${ANDROID_SDK_TOOLS}"
  DEFINES+=" android_sdk_version=${ANDROID_SDK_VERSION}"
  DEFINES+=" android_toolchain=${ANDROID_TOOLCHAIN}"
  if [[ -n "$CHROME_ANDROID_WEBVIEW_ENABLE_DMPROF" ]]; then
    DEFINES+=" disable_debugallocation=1"
    DEFINES+=" android_full_debug=1"
    DEFINES+=" android_use_tcmalloc=1"
  fi
  if [[ -n "$CHROME_ANDROID_WEBVIEW_OFFICIAL_BUILD" ]]; then
    DEFINES+=" logging_like_official_build=1"
    DEFINES+=" tracing_like_official_build=1"
  fi
  export GYP_DEFINES="${DEFINES}"

  export GYP_GENERATORS="android"

  export GYP_GENERATOR_FLAGS="${GYP_GENERATOR_FLAGS} default_target=All"
  export GYP_GENERATOR_FLAGS="${GYP_GENERATOR_FLAGS} limit_to_target_all=1"

  export CHROMIUM_GYP_FILE="${CHROME_SRC}/android_webview/all_webview.gyp"
}
