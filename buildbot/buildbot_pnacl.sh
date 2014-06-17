#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o xtrace
set -o nounset
set -o errexit

# Tell build.sh and test.sh that we're a bot.
export PNACL_BUILDBOT=true
# Make TC bots print all log output to console (this variable only affects
# build.sh output)
export PNACL_VERBOSE=true

# This affects trusted components of gyp and scons builds.
# The setting OPT reuslts in optimized/release trusted executables,
# the setting DEBUG results in unoptimized/debug trusted executables
BUILD_MODE_HOST=OPT
# If true, terminate script when first scons error is encountered.
FAIL_FAST=${FAIL_FAST:-true}
# This remembers when any build steps failed, but we ended up continuing.
RETCODE=0
# For scons builds an empty target indicates all targets should be built.
# This does not run any tests, though.
# Large tests are not included in this group because they cannot be run
# in parallel (they can do things like bind to local TCP ports). Large
# tests are run separately in the functions below.
readonly SCONS_EVERYTHING=""
readonly SCONS_S_M="small_tests medium_tests"
readonly SCONS_S_M_IRT="small_tests_irt medium_tests_irt"

# Using skip_nonstable_bitcode=1 here disables the tests for zero-cost C++
# exception handling, which don't pass for Non-SFI mode yet because we
# don't build libgcc_eh for Non-SFI mode.
# TODO(mseaborn): Run small_tests_irt with nonsfi_nacl=1 when it passes,
# instead of the following whitelist of tests.
readonly SCONS_NONSFI_TESTS="\
    run_hello_world_test_irt \
    run_float_test_irt \
    run_malloc_realloc_calloc_free_test_irt \
    run_dup_test_irt \
    run_syscall_test_irt \
    run_getpid_test_irt \
    toolchain_tests_irt \
    skip_nonstable_bitcode=1"
readonly SCONS_NONSFI="nonsfi_nacl=1 ${SCONS_NONSFI_TESTS}"
# Extra non-IRT-using test to run for x86-32 and ARM on toolchain bots.
# TODO(mseaborn): Run this on the main bots after the toolchain revision is
# updated.
readonly SCONS_NONSFI_TC="\
    ${SCONS_NONSFI} \
    run_clock_get_test \
    run_dup_test \
    run_hello_world_test \
    run_mmap_test \
    run_nanosleep_test \
    run_printf_test \
    run_pwrite_test"

# subset of tests used on toolchain builders
readonly SCONS_TC_TESTS="small_tests medium_tests"

readonly SCONS_COMMON="./scons --verbose bitcode=1"
readonly UP_DOWN_LOAD="buildbot/file_up_down_load.sh"
# This script is used by toolchain bots (i.e. tc-xxx functions)
readonly PNACL_BUILD="pnacl/build.sh"
readonly DRIVER_TESTS="pnacl/driver/tests/driver_tests.py"


tc-build-translator() {
  echo @@@BUILD_STEP compile_translator@@@
  ${PNACL_BUILD} translator-clean-all
  ${PNACL_BUILD} translator-all
}

tc-prune-translator-pexes() {
  echo @@@BUILD_STEP prune_translator_pexe@@@
  ${PNACL_BUILD} translator-prune
}

tc-archive-translator() {
  echo @@@BUILD_STEP archive_translator@@@
  ${PNACL_BUILD} translator-tarball pnacl-translator.tgz
  ${UP_DOWN_LOAD} UploadToolchainTarball ${BUILDBOT_GOT_REVISION} \
      pnacl_translator pnacl-translator.tgz

  echo @@@BUILD_STEP upload_translator_package_info@@@
  ${NATIVE_PYTHON} build/package_version/package_version.py archive \
      --archive-package=pnacl_translator \
      pnacl-translator.tgz@https://storage.googleapis.com/nativeclient-archive2/toolchain/${BUILDBOT_GOT_REVISION}/naclsdk_pnacl_translator.tgz

  ${NATIVE_PYTHON} build/package_version/package_version.py --annotate upload \
      --upload-package=pnacl_translator --revision=${BUILDBOT_GOT_REVISION}
}


# extract the relevant scons flags for reporting
relevant() {
  for i in "$@" ; do
    case $i in
      use_sandboxed_translator=1)
        echo -n "sbtc "
        ;;
      do_not_run_tests=1)
        echo -n "no_tests "
        ;;
      pnacl_generate_pexe=0)
        echo -n "no_pexe "
        ;;
      translate_fast=1)
        echo -n "fast "
        ;;
      --nacl_glibc)
        echo -n "glibc "
        ;;
    esac
  done
}

# called when a scons invocation fails
handle-error() {
  RETCODE=1
  echo "@@@STEP_FAILURE@@@"
  if ${FAIL_FAST} ; then
    echo "FAIL_FAST enabled"
    exit 1
  fi
}

# Clear out object, and temporary directories.
clobber() {
  echo "@@@BUILD_STEP clobber@@@"
  rm -rf scons-out ../xcodebuild ../out
}

# Generate filenames for arm bot uploads and downloads
NAME_ARM_UPLOAD() {
  echo -n "${BUILDBOT_BUILDERNAME}/${BUILDBOT_GOT_REVISION}"
}

NAME_ARM_DOWNLOAD() {
  echo -n "${BUILDBOT_TRIGGERED_BY_BUILDERNAME}/${BUILDBOT_GOT_REVISION}"
}

NAME_ARM_TRY_UPLOAD() {
  echo -n "${BUILDBOT_BUILDERNAME}/"
  echo -n "${BUILDBOT_SLAVENAME}/"
  echo -n "${BUILDBOT_BUILDNUMBER}"
}
NAME_ARM_TRY_DOWNLOAD() {
  echo -n "${BUILDBOT_TRIGGERED_BY_BUILDERNAME}/"
  echo -n "${BUILDBOT_TRIGGERED_BY_SLAVENAME}/"
  echo -n "${BUILDBOT_TRIGGERED_BY_BUILDNUMBER}"
}


prune-scons-out() {
  find scons-out/ \
    \( -name '*.o' -o -name '*.bc' -o -name 'test_results' \) \
    -print0 | xargs -0 rm -rf
}

# Tar up the executables which are shipped to the arm HW bots
archive-for-hw-bots() {
  local name=$1
  local try=$2

  echo "@@@BUILD_STEP tar_generated_binaries@@@"
  prune-scons-out

  # delete nexes from pexe mode directories to force translation
  # TODO(dschuff) enable this once we can translate on the hw bots
  #find scons-out/*pexe*/ -name '*.nexe' -print0 | xargs -0 rm -f
  tar cvfz arm-scons.tgz scons-out/*arm*

  echo "@@@BUILD_STEP archive_binaries@@@"
  if [[ ${try} == "try" ]] ; then
    ${UP_DOWN_LOAD} UploadArmBinariesForHWBotsTry ${name} arm-scons.tgz
  else
    ${UP_DOWN_LOAD} UploadArmBinariesForHWBots ${name} arm-scons.tgz
  fi
}

# Untar archived executables for HW bots
unarchive-for-hw-bots() {
  local name=$1
  local try=$2

  echo "@@@BUILD_STEP fetch_binaries@@@"
  if [[ ${try} == "try" ]] ; then
    ${UP_DOWN_LOAD} DownloadArmBinariesForHWBotsTry ${name} arm-scons.tgz
  else
    ${UP_DOWN_LOAD} DownloadArmBinariesForHWBots ${name} arm-scons.tgz
  fi

  echo "@@@BUILD_STEP untar_binaries@@@"
  rm -rf scons-out/
  tar xvfz arm-scons.tgz --no-same-owner
}

# Build with gyp - this only exercises the trusted TC and hence this only
# makes sense to run for ARM.
gyp-arm-build() {
  local gypmode="Release"
  if [ "${BUILD_MODE_HOST}" = "DEBUG" ] ; then
      gypmode="Debug"
  fi
  local toolchain_dir=native_client/toolchain/linux_x86/arm_trusted
  local extra="-isystem ${toolchain_dir}/usr/include \
               -Wl,-rpath-link=${toolchain_dir}/lib/arm-linux-gnueabihf \
               -L${toolchain_dir}/lib \
               -L${toolchain_dir}/lib/arm-linux-gnueabihf \
               -L${toolchain_dir}/usr/lib \
               -L${toolchain_dir}/usr/lib/arm-linux-gnueabihf"
  # Setup environment for arm.

  export AR=arm-linux-gnueabihf-ar
  export AS=arm-linux-gnueabihf-as
  export CC="arm-linux-gnueabihf-gcc ${extra} "
  export CXX="arm-linux-gnueabihf-g++ ${extra} "
  export LD="arm-linux-gnueabihf-g++ ${extra} "
  export RANLIB=arm-linux-gnueabihf-ranlib
  export SYSROOT
  export GYP_DEFINES="target_arch=arm \
    sysroot=${toolchain_dir} \
    linux_use_tcmalloc=0 armv7=1 arm_thumb=1"
  export GYP_GENERATORS=make

  # NOTE: this step is also run implicitly as part of
  #        gclient runhooks --force
  #       it uses the exported env vars so we have to run it again
  #
  echo "@@@BUILD_STEP gyp_configure [${gypmode}]@@@"
  cd ..
  native_client/build/gyp_nacl native_client/build/all.gyp
  cd native_client

  echo "@@@BUILD_STEP gyp_compile [${gypmode}]@@@"
  make -C .. -k -j8 V=1 BUILDTYPE=${gypmode}
}

build-sbtc-prerequisites() {
  local platform=$1
  # Sandboxed translators currently only require irt_core since they do not
  # use PPAPI.
  ${SCONS_COMMON} platform=${platform} sel_ldr sel_universal irt_core
}

# Run a single invocation of scons as its own buildbot stage and handle errors
scons-stage-irt() {
  local platform=$1
  local extra=$2
  local test=$3
  local info="$(relevant ${extra})"
  # TODO(robertm): do we really need both nacl and nacl_irt_test
  local mode="--mode=opt-host,nacl,nacl_irt_test"
  if [ "${BUILD_MODE_HOST}" = "DEBUG" ] ; then
      mode="--mode=dbg-host,nacl,nacl_irt_test"
  fi

  # We truncate the test list ($test) because long BUILD_STEP strings cause
  # Buildbot to fail (because the Buildbot master tries to use the string
  # as a filename).
  echo "@@@BUILD_STEP scons-irt [${platform}] [${test:0:100}] [${info}]@@@"
  ${SCONS_COMMON} ${extra} ${mode} platform=${platform} ${test} || handle-error
}

# Run a single invocation of scons as its own buildbot stage and handle errors
scons-stage-noirt() {
  local platform=$1
  local extra=$2
  local test=$3
  local info="$(relevant ${extra})"
  local mode="--mode=opt-host,nacl"
  if [ "${BUILD_MODE_HOST}" = "DEBUG" ] ; then
      mode="--mode=dbg-host,nacl"
  fi

  echo "@@@BUILD_STEP scons [${platform}] [${test}] [${info}]@@@"
  ${SCONS_COMMON} ${extra} ${mode} platform=${platform} ${test} || handle-error
}


driver-tests() {
  local arch=$1
  echo "@@@BUILD_STEP driver_tests ${arch}@@@"
  ${DRIVER_TESTS} --platform="${arch}" || handle-error
}


# QEMU upload bot runs this function, and the hardware download bot runs
# mode-buildbot-arm-hw
mode-buildbot-arm() {
  FAIL_FAST=false
  # "force_emulator=" disables use of QEMU, which enables building
  # tests which don't work under QEMU.
  local qemuflags="-j8 -k do_not_run_tests=1 force_emulator="

  clobber

  gyp-arm-build

  # Sanity check
  scons-stage-noirt "arm" "-j8" "run_hello_world_test"

  # Don't run the rest of the tests on qemu, only build them.
  # QEMU is too flaky for the main waterfall

  # Normal pexe mode tests
  scons-stage-noirt "arm" "${qemuflags}" "${SCONS_EVERYTHING}"
  # This extra step is required to translate the pexes (because translation
  # happens as part of CommandSelLdrTestNacl and not part of the
  # build-everything step)
  scons-stage-noirt "arm" "${qemuflags}" "${SCONS_S_M}"
  # Large tests cannot be run in parallel
  scons-stage-noirt "arm" "${qemuflags} -j1" "large_tests"

  # also run some tests with the irt
  scons-stage-irt "arm" "${qemuflags}" "${SCONS_S_M_IRT}"

  # non-pexe-mode tests
  scons-stage-noirt "arm" "${qemuflags} pnacl_generate_pexe=0" "nonpexe_tests"

  build-sbtc-prerequisites "arm"

  scons-stage-irt "arm" \
    "${qemuflags} use_sandboxed_translator=1 translate_in_build_step=0" \
    "toolchain_tests"
  scons-stage-irt "arm" \
    "${qemuflags} use_sandboxed_translator=1 translate_fast=1 \
       translate_in_build_step=0" \
    "toolchain_tests"

  # Test Non-SFI Mode.
  scons-stage-irt "arm" "${qemuflags}" "${SCONS_NONSFI}"
}

mode-buildbot-arm-hw() {
  FAIL_FAST=false
  local hwflags="-j2 -k naclsdk_validate=0 built_elsewhere=1"

  scons-stage-noirt "arm" "${hwflags}" "${SCONS_S_M}"
  # Large tests cannot be run in parallel
  scons-stage-noirt "arm" "${hwflags} -j1" "large_tests"

  # also run some tests with the irt
  scons-stage-irt "arm" "${hwflags}" "${SCONS_S_M_IRT}"

  scons-stage-noirt "arm" "${hwflags} pnacl_generate_pexe=0" "nonpexe_tests"
  scons-stage-irt "arm" \
    "${hwflags} use_sandboxed_translator=1 translate_in_build_step=0" \
    "toolchain_tests"
  scons-stage-irt "arm" \
    "${hwflags} use_sandboxed_translator=1 translate_fast=1 \
       translate_in_build_step=0" \
    "toolchain_tests"

  # Test Non-SFI Mode.
  scons-stage-irt "arm" "${hwflags}" "${SCONS_NONSFI}"
}

mode-trybot-qemu() {
  # Build and actually run the arm tests under qemu, except
  # sandboxed translation. Hopefully that's a good tradeoff between
  # flakiness and cycle time.
  FAIL_FAST=false
  local qemuflags="-j4 -k"
  clobber
  gyp-arm-build

  scons-stage-noirt "arm" "${qemuflags}" "${SCONS_EVERYTHING}"
  # Large tests cannot be run in parallel
  scons-stage-noirt "arm" "${qemuflags} -j1" "${SCONS_S_M} large_tests"

  # also run some tests with the irt
  scons-stage-irt "arm" "${qemuflags}" "${SCONS_S_M_IRT}"

  # non-pexe tests
  scons-stage-noirt "arm" "${qemuflags} pnacl_generate_pexe=0" "nonpexe_tests"

  # Test Non-SFI Mode.
  scons-stage-irt "arm" "${qemuflags}" "${SCONS_NONSFI}"
}

mode-buildbot-arm-dbg() {
  BUILD_MODE_HOST=DEDUG
  mode-buildbot-arm
  archive-for-hw-bots $(NAME_ARM_UPLOAD) regular
}

mode-buildbot-arm-opt() {
  mode-buildbot-arm
  archive-for-hw-bots $(NAME_ARM_UPLOAD) regular
}

mode-buildbot-arm-try() {
  mode-buildbot-arm
  archive-for-hw-bots $(NAME_ARM_TRY_UPLOAD) try
}

# NOTE: the hw bots are too slow to build stuff on so we just
#       use pre-built executables
mode-buildbot-arm-hw-dbg() {
  BUILD_MODE_HOST=DEDUG
  unarchive-for-hw-bots $(NAME_ARM_DOWNLOAD)  regular
  mode-buildbot-arm-hw
}

mode-buildbot-arm-hw-opt() {
  unarchive-for-hw-bots $(NAME_ARM_DOWNLOAD)  regular
  mode-buildbot-arm-hw
}

mode-buildbot-arm-hw-try() {
  unarchive-for-hw-bots $(NAME_ARM_TRY_DOWNLOAD)  try
  mode-buildbot-arm-hw
}

# These 2 functions are also suitable for local TC sanity testing.
tc-tests-all() {
  local is_try=$1

  local label="pnacl_newlib_dir=toolchain/${PNACL_TOOLCHAIN_DIR}"
  local scons_flags="-k skip_trusted_tests=1 -j8 ${label}"

  # newlib
  for arch in x86-32 x86-64 arm; do
    driver-tests "${arch}"
    scons-stage-noirt "$arch" "${scons_flags}" "${SCONS_TC_TESTS}"
    # Large tests cannot be run in parallel
    scons-stage-noirt "$arch" "${scons_flags} -j1" "large_tests"
    scons-stage-noirt "$arch" "${scons_flags} pnacl_generate_pexe=0" \
        "nonpexe_tests"
  done

  # Small set of sbtc tests w/ and without translate_fast=1.
  scons-stage-irt "x86-32" "${scons_flags} use_sandboxed_translator=1" \
    "toolchain_tests"
  scons-stage-irt "x86-32" \
    "${scons_flags} use_sandboxed_translator=1 translate_fast=1" \
    "toolchain_tests"
  scons-stage-irt "x86-64" "${scons_flags} use_sandboxed_translator=1" \
    "toolchain_tests"
  scons-stage-irt "x86-64" \
    "${scons_flags} use_sandboxed_translator=1 translate_fast=1" \
    "toolchain_tests"
  # Smaller set of sbtc tests for ARM because qemu is flaky.
  scons-stage-irt "arm" "${scons_flags} use_sandboxed_translator=1" \
    "run_hello_world_test"
  scons-stage-irt "arm" \
    "${scons_flags} use_sandboxed_translator=1 translate_fast=1" \
    "run_hello_world_test"

  # Test Non-SFI Mode.
  scons-stage-irt "x86-32" "${scons_flags}" "${SCONS_NONSFI_TC}"
  scons-stage-irt "arm" "${scons_flags}" "${SCONS_NONSFI_TC}"

  # Test unsandboxed mode.
  scons-stage-irt "x86-32" "${scons_flags}" "pnacl_unsandboxed=1" \
    "run_hello_world_test_irt"

  # Run the GCC torture tests just for x86-32.  Testing a single
  # architecture gives good coverage without taking too long.  We
  # don't test x86-64 here because some of the torture tests fail on
  # the x86-64 toolchain trybot (though not the buildbots, apparently
  # due to a hardware difference:
  # https://code.google.com/p/nativeclient/issues/detail?id=3697).
  # Build the SDK libs first so that linking will succeed.
  echo "@@@BUILD_STEP sdk libs @@@"
  ${PNACL_BUILD} sdk

  echo "@@@BUILD_STEP torture_tests x86-32 @@@"
  tools/toolchain_tester/torture_test.py pnacl x86-32 --verbose \
      --concurrency=8 || handle-error
}

tc-tests-fast() {
  local arch="$1"
  local scons_flags="-k skip_trusted_tests=1"

  driver-tests "${arch}"

  scons-stage-noirt "${arch}" "${scons_flags} -j8" "${SCONS_TC_TESTS}"
  # Large tests cannot be run in parallel
  scons-stage-noirt "${arch}" "${scons_flags} -j1" "large_tests"
  scons-stage-noirt "${arch}" "${scons_flags} -j8 pnacl_generate_pexe=0" \
    "nonpexe_tests"
}

mode-buildbot-tc-x8664-linux() {
  local is_try=$1
  FAIL_FAST=false
  export PNACL_TOOLCHAIN_DIR=linux_x86/pnacl_newlib
  export PNACL_PRUNE=true

  tc-build-translator
  if ! ${is_try} ; then
    tc-prune-translator-pexes
    tc-archive-translator
  fi
  HOST_ARCH=x86_64 tc-tests-all ${is_try}
}

mode-buildbot-tc-x8632-linux() {
  local is_try=$1
  FAIL_FAST=false
  export PNACL_TOOLCHAIN_DIR=linux_x86/pnacl_newlib

  # For now, just use this bot to test a pure 32 bit build.
  HOST_ARCH=x86_32 tc-tests-fast "x86-32"
}


######################################################################
# On Windows, this script is invoked from a batch file.
# The inherited PWD environmental variable is a Windows-style path.
# This can cause problems with pwd and bash. This line fixes it.
cd -P .

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi


if [[ $# -eq 0 ]] ; then
  echo "you must specify a mode on the commandline:"
  exit 1
fi

if [ "$(type -t $1)" != "function" ]; then
  Usage
  echo "ERROR: unknown mode '$1'." >&2
  exit 1
fi

"$@"

if [[ ${RETCODE} != 0 ]]; then
  echo "@@@BUILD_STEP summary@@@"
  echo There were failed stages.
  exit ${RETCODE}
fi
