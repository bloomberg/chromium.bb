#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o xtrace
set -o nounset
set -o errexit

# If true, terminate script when first scons error is encountered.
FAIL_FAST=${FAIL_FAST:-true}
# This remembers when any build steps failed, but we ended up continuing.
RETCODE=0

readonly SCONS_COMMON="./scons --verbose bitcode=1"
readonly UP_DOWN_LOAD="buildbot/file_up_down_load.sh"

# extract the relevant scons flags for reporting
relevant() {
  for i in "$@" ; do
    case $i in
      nacl_pic=1)
        echo $i
        ;;
      use_sandboxed_translator=1)
        echo $i
        ;;
      do_not_run_tests=1)
        echo $i
        ;;
      pnacl_generate_pexe=1)
        echo $i
        ;;
      --nacl_glibc)
        echo $i
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
  rm -rf scons-out ../xcodebuild ../sconsbuild ../out
  # Try to clobber /tmp/ contents to clear temporary chrome files.
  rm -rf /tmp/.org.chromium.Chromium.*
}

# This is the first thing you want to run on the bots to install the toolchains
install-lkgr-toolchains() {
  echo "@@@BUILD_STEP install_toolchains@@@"
  gclient runhooks --force
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

# Tar up the executables which are shipped to the arm HW bots
archive-for-hw-bots() {
  local name=$1
  local try=$2

  echo "@@@BUILD_STEP tar_generated_binaries@@@"
  # clean out a bunch of files that are not needed
  find scons-out/ \
    \( -name '*.o' -o -name '*.bc' -o -name 'test_results' \) \
    -print0 | xargs -0 rm -rf

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
  gypmode=$1
  TOOLCHAIN_DIR=native_client/toolchain/linux_arm-trusted
  EXTRA="-isystem ${TOOLCHAIN_DIR}/usr/include \
         -Wl,-rpath-link=${TOOLCHAIN_DIR}/lib/arm-linux-gnueabi \
         -L${TOOLCHAIN_DIR}/lib \
         -L${TOOLCHAIN_DIR}/lib/arm-linux-gnueabi \
         -L${TOOLCHAIN_DIR}/usr/lib \
         -L${TOOLCHAIN_DIR}/usr/lib/arm-linux-gnueabi"
  # Setup environment for arm.

  export AR=arm-linux-gnueabi-ar
  export AS=arm-linux-gnueabi-as
  export CC="arm-linux-gnueabi-gcc-4.5 ${EXTRA} "
  export CXX="arm-linux-gnueabi-g++-4.5 ${EXTRA} "
  export LD=arm-linux-gnueabi-ld
  export RANLIB=arm-linux-gnueabi-ranlib
  export SYSROOT
  export GYP_DEFINES="target_arch=arm \
    sysroot=${TOOLCHAIN_DIR} \
    linux_use_tcmalloc=0 armv7=1 arm_thumb=1"
  export GYP_GENERATOR=make

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


ad-hoc-shared-lib-tests() {
  local platform=$1
  # TODO(robertm): make this accessible by the utman script so that this get
  # http://code.google.com/p/nativeclient/issues/detail?id=1647
  echo "@@@BUILD_STEP fake_shared_libs@@@"
  { pushd  tests/pnacl_ld_example/ &&
    make -f Makefile.pnacl clean &&
    make -f Makefile.pnacl preparation.${platform} &&
    make -f Makefile.pnacl run.${platform} &&
    make -f Makefile.pnacl run2.${platform} &&
    popd
  } || handle-error
}

build-sbtc-prerequisites() {
  local platform=$1
  # Sandboxed translators currently only require irt_core since they do not
  # use PPAPI.
  ${SCONS_COMMON} platform=${platform} sel_ldr sel_universal irt_core
}

# Run a single invocation of scons as its own buildbot stage and handle errors
scons-stage() {
  local platform=$1
  local extra=$2
  local test=$3
  echo "@@@BUILD_STEP scons [${platform}] [${test}] \
[$(relevant ${extra})]@@@"
  ${SCONS_COMMON} ${extra} platform=${platform} ${test} || handle-error
}

# Do separate stages with parallel build and sequential test. Callers should
# pass -jN as part of $extra
scons-build-test() {
  local platform=$1
  local extra=$2
  local test=$3
  echo "@@@BUILD_STEP scons [${platform}] [${test}] \
[$(relevant ${extra})]@@@"
  ${SCONS_COMMON} ${extra} do_not_run_tests=1 platform=${platform} \
    ${test} || handle-error
  # -j1 overrides any -jM in extra
  ${SCONS_COMMON} ${extra} -j1 platform=${platform} ${test} || handle-error
}

single-browser-test() {
  local platform=$1
  local extra=$2
  local test=$3
  # Build in parallel (assume -jN specified in extra), but run sequentially.
  # If we do not run tests sequentially, some may fail. E.g.,
  # http://code.google.com/p/nativeclient/issues/detail?id=2019
  scons-build-test ${platform} "${extra} browser_headless=1 SILENT=1" \
      ${test}
}

browser-tests() {
  local platform=$1
  local extra=$2
  if [[ "${extra}" =~ --nacl_glibc ]]; then
    # For glibc, only non-pexe mode works for now.
    # We need to ensure psos are translated before running.
    # E.g., run_pm_manifest_file_chrome_browser_test relies on
    # libimc, libweak_ref, etc.
    single-browser-test ${platform} "${extra} pnacl_generate_pexe=0" \
        "chrome_browser_tests"
  else
    # Otherwise, try pexe mode.
    single-browser-test ${platform} "${extra}" "chrome_browser_tests"
  fi
}

# This function is shared between x86-32 and x86-64. All building and testing
# is done on the same bot. Try runs are identical to buildbot runs
mode-buildbot-x86() {
  local bits=$1
  FAIL_FAST=false
  clobber
  install-lkgr-toolchains

  # For now, just build what we want to test, until pexe nmf issues are solved
  scons-build-test "x86-${bits}" "--mode=opt-host,nacl -j8 -k" "smoke_tests"

  # Build and run non-pexe tests
  # For now, build everything until nmf dependency issues are resolved
  scons-stage "x86-${bits}" "--mode=opt-host,nacl -j8 -k pnacl_generate_pexe=0"\
    ""
  scons-stage "x86-${bits}" "--mode=opt-host,nacl -k pnacl_generate_pexe=0" \
    "nonpexe_tests"

  scons-build-test "x86-${bits}" \
    "--mode=opt-host,nacl -k pnacl_generate_pexe=0 nacl_pic=1" \
    "small_tests medium_tests large_tests"

  # sandboxed translation
  build-sbtc-prerequisites x86-${bits}
  scons-build-test "x86-${bits}" "--mode=opt-host,nacl -j8 -k \
    use_sandboxed_translator=1" "smoke_tests"

  browser-tests "x86-${bits}" "--mode=opt-host,nacl -j8 -k"
}

# QEMU upload bot runs this function, and the hardware download bot runs
# mode-buildbot-arm-hw
mode-buildbot-arm() {
  FAIL_FAST=false
  local mode=$1
  local qemuflags="${mode} -j8 -k"

  clobber
  install-lkgr-toolchains

  gyp-arm-build Release

  scons-build-test "arm" "${qemuflags}" "small_tests medium_tests large_tests"
  scons-build-test "arm" "${qemuflags} pnacl_generate_pexe=0 nacl_pic=1" \
    "small_tests medium_tests large_tests"

  # non-pexe tests
  scons-stage "arm" "${mode} ${qemuflags} pnacl_generate_pexe=0" "nonpexe_tests"

  # Full test suite of translator for ARM is too flaky on QEMU
  # http://code.google.com/p/nativeclient/issues/detail?id=2581
  # Run a subset here.
  # For now just do toolchain_tests as we have been.
  # TODO(dschuff): Enable more sandboxed tests on hardware
  build-sbtc-prerequisites "arm"

  scons-stage "arm" \
    "${qemuflags} use_sandboxed_translator=1 translate_in_build_step=0 \
    do_not_run_tests=1" "toolchain_tests"

  browser-tests "arm" "${mode}"
  # Disabled for now as it broke when we switched to gold for final
  # linking. We may remove this permanently as it is not doing all that
  # much anyway.
  # ad-hoc-shared-lib-tests "arm"
}

mode-buildbot-arm-hw() {
  FAIL_FAST=false
  local mode=$1
  local hwflags="${mode} -j2 -k naclsdk_validate=0 built_elsewhere=1"

  scons-stage "arm" "${hwflags}" "small_tests medium_tests large_tests"
  scons-stage "arm" "${hwflags} pnacl_generate_pexe=0" "nonpexe_tests"
  scons-stage "arm" \
    "${hwflags} use_sandboxed_translator=1 translate_in_build_step=0" \
    "toolchain_tests"
  # TODO(dschuff): consider running pic tests here as well
  browser-tests "arm" "${hwflags}"
}

mode-trybot-qemu() {
  # For now, just run the qemu build but don't upload anything.
  # TODO(dschuff) split the try functionality such that the uploading
  # bot will not run (many) tests, and the non-uploading bot will run
  # tests under qemu. This should hopefully reduce cycle time.
  mode-buildbot-arm "--mode=opt-host,nacl"
}

mode-buildbot-arm-dbg() {
  mode-buildbot-arm "--mode=dbg-host,nacl"
  archive-for-hw-bots $(NAME_ARM_UPLOAD) regular
}

mode-buildbot-arm-opt() {
  mode-buildbot-arm "--mode=opt-host,nacl"
  archive-for-hw-bots $(NAME_ARM_UPLOAD) regular
}

mode-buildbot-arm-try() {
  mode-buildbot-arm "--mode=opt-host,nacl"
  archive-for-hw-bots $(NAME_ARM_TRY_UPLOAD) try
}

# NOTE: the hw bots are too slow to build stuff on so we just
#       use pre-built executables
mode-buildbot-arm-hw-dbg() {
  unarchive-for-hw-bots $(NAME_ARM_DOWNLOAD)  regular
  mode-buildbot-arm-hw "--mode=dbg-host,nacl"
}

mode-buildbot-arm-hw-opt() {
  unarchive-for-hw-bots $(NAME_ARM_DOWNLOAD)  regular
  mode-buildbot-arm-hw "--mode=opt-host,nacl"
}

mode-buildbot-arm-hw-try() {
  unarchive-for-hw-bots $(NAME_ARM_TRY_DOWNLOAD)  try
  mode-buildbot-arm-hw "--mode=opt-host,nacl"
}

mode-test-all() {
  test-all-newlib "$@"
  test-all-glibc "$@"
}

# NOTE: clobber and toolchain setup to be done manually, since this is for
# testing a locally built toolchain.
# This runs tests concurrently, so may be more difficult to parse logs.
test-all-newlib() {
  local concur=$1

  # turn verbose mode off
  set +o xtrace

  # At least clobber scons-out before building and running the tests though.
  echo "@@@BUILD_STEP clobber@@@"
  rm -rf scons-out

  # First build everything.
  echo "@@@BUILD_STEP scons build @@@"
  scons-stage "arm" "--mode=opt-host,nacl -j${concur}" ""
  scons-stage "x86-32" "--mode=opt-host,nacl -j${concur}" ""
  scons-stage "x86-64" "--mode=opt-host,nacl -j${concur}" ""
  # Then test everything.
  echo "@@@BUILD_STEP scons smoke_tests @@@"
  scons-stage "arm" "--mode=opt-host,nacl -j${concur}" "smoke_tests"
  scons-stage "x86-32" "--mode=opt-host,nacl -j${concur}" "smoke_tests"
  scons-stage "x86-64" "--mode=opt-host,nacl -j${concur}" "smoke_tests"
  # browser tests.
  browser-tests "arm" "--verbose --mode=opt-host,nacl -j${concur}"
  browser-tests "x86-32" "--verbose --mode=opt-host,nacl -j${concur}"
  browser-tests "x86-64" "--verbose --mode=opt-host,nacl -j${concur}"
}

test-all-glibc() {
  # TODO(pdox): Add GlibC tests
  ad-hoc-shared-lib-tests "arm"
}

######################################################################
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
