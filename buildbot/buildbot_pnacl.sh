#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
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

# extract the relavant scons flags for reporting
relevant() {
  for i in "$@" ; do
    case $i in
      nacl_pic=1)
        echo $i
        ;;
      use_sandboxed_translator=1)
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

# clear out object, temporary, and toolchain directories
clobber() {
  echo "@@@BUILD_STEP clobber@@@"
  rm -rf scons-out toolchain compiler hg ../xcodebuild ../sconsbuild ../out \
    src/third_party/nacl_sdk/arm-newlib
  # Try to clobber /tmp/ contents to clear temporary chrome files.
  rm -rf /tmp/.org.chromium.Chromium.*
}

# This is the first thing you want to run on the bots to install the toolchains
install-lkgr-toolchains() {
  echo "@@@BUILD_STEP install_toolchains@@@"
  gclient runhooks --force
}

# copy data to well known archive server
push-data-to-archive-server() {
  buildbot/gsutil.sh -h Cache-Control:no-cache cp -a public-read \
    $1 \
    gs://$2
}

# copy data from well known archive server
pull-data-from-archive-server() {
  curl -L \
     http://commondatastorage.googleapis.com/$1\
     -o $2
}

# Tar up the executables which are shipped to the arm HW bots
archive-for-hw-bots() {
  local target=$1
  echo "@@@BUILD_STEP tar_generated_binaries@@@"
  # clean out a bunch of files that are not needed
  find scons-out/ \
    \( -name '*.[so]' -o -name '*.bc' -o -name '*.pexe' -o -name '*.ll' \) \
    -print0 | xargs -0 rm -f

  tar cvfz arm.tgz scons-out/

  echo "@@@BUILD_STEP archive_binaries@@@"
  push-data-to-archive-server arm.tgz ${target}
}

# Untar archived executables for HW bots
unarchive-for-hw-bots() {
  local source=$1

  echo "@@@BUILD_STEP fetch_binaries@@@"
  pull-data-from-archive-server ${source} arm.tgz

  echo "@@@BUILD_STEP untar_binaries@@@"
  rm -rf scons-out/
  tar xvfz arm.tgz --no-same-owner
}

# Build with gyp - this only exercises the trusted TC and hence this only
# makes sense to run for ARM.
gyp-arm-build() {
  gypmode=$1
  TOOLCHAIN_DIR=native_client/toolchain/linux_arm-trusted
  EXTRA="-isystem ${TOOLCHAIN_DIR}/usr/include \
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
  pushd  tests/pnacl_ld_example/
  make -f Makefile.pnacl clean
  make -f Makefile.pnacl preparation.${platform}
  make -f Makefile.pnacl run.${platform}
  make -f Makefile.pnacl run2.${platform}
  popd
}

build-sbtc-prerequisites() {
  local platform=$1
  # Sandboxed translators currently only require irt_core since they do not
  # use PPAPI.
  ${SCONS_COMMON} platform=${platform} sel_ldr sel_universal irt_core
}

single-scons-test() {
  local platform=$1
  local extra=$2
  local test=$3
  echo "@@@BUILD_STEP scons [${platform}] [${test}] [$(relevant ${extra})]@@@"
  ${SCONS_COMMON} ${extra} platform=${platform} ${test} || handle-error
}

single-browser-test() {
  local platform=$1
  local extra=$2
  local test=$3
  echo "@@@BUILD_STEP scons [${platform}] [${test}] [$(relevant ${extra})]@@@"
  ${SCONS_COMMON} ${extra} browser_headless=1 SILENT=1 platform=${platform} \
    ${test} || handle-error
}

scons-tests() {
  local platform=$1
  local extra=$2
  local test=$3

  single-scons-test ${platform} "${extra}" "${test}"
  # Run the scons PIC tests on ARM only, since
  # the GlibC scons tests already test PIC mode for X86.
  # BUG= http://code.google.com/p/nativeclient/issues/detail?id=1081
  if [ "${platform}" == arm ]; then
    single-scons-test ${platform} "${extra} nacl_pic=1" "${test}"
  fi
  build-sbtc-prerequisites ${platform}
  single-scons-test ${platform} "${extra} use_sandboxed_translator=1" "${test}"
}

scons-tests-no-translator() {
  local platform=$1
  local extra=$2
  local test=$3
  single-scons-test ${platform} "${extra}" "${test}"
  single-scons-test ${platform} "${extra} nacl_pic=1" "${test}"
}

browser-tests() {
  local platform=$1
  local extra=$2
  local test="chrome_browser_tests"
  single-browser-test ${platform} "${extra}" "${test}"
}

######################################################################
# NOTE: these trybots are expected to diverge some more hence the code
#       duplication
mode-trybot-arm() {
  FAIL_FAST=false
  clobber
  install-lkgr-toolchains
  scons-tests "arm" "--mode=opt-host,nacl -j8 -k" "smoke_tests"
  browser-tests "arm" "--mode=opt-host,nacl -k"
  ad-hoc-shared-lib-tests "arm"
}

mode-trybot-x8632() {
  FAIL_FAST=false
  clobber
  install-lkgr-toolchains
  scons-tests "x86-32" "--mode=opt-host,nacl -j8 -k" "smoke_tests"
  browser-tests "x86-32" "--mode=opt-host,nacl -k"
}

mode-trybot-x8664() {
  FAIL_FAST=false
  clobber
  install-lkgr-toolchains
  scons-tests "x86-64" "--mode=opt-host,nacl -j8 -k" "smoke_tests"
  browser-tests "x86-64" "--mode=opt-host,nacl -k"
}

mode-buildbot-x8632() {
  FAIL_FAST=false
  clobber
  install-lkgr-toolchains
  # First build everything
  scons-tests "x86-32" "--mode=opt-host,nacl -j8 -k" ""
  # Then test (not all nexes which are build are also tested)
  scons-tests "x86-32" "--mode=opt-host,nacl -k" "smoke_tests"
  browser-tests "x86-32" "--mode=opt-host,nacl -k"
}

mode-buildbot-x8664() {
  FAIL_FAST=false
  clobber
  install-lkgr-toolchains
  # First build everything
  scons-tests "x86-64" "--mode=opt-host,nacl -j8 -k" ""
  # Then test (not all nexes which are build are also tested)
  scons-tests "x86-64" "--mode=opt-host,nacl -k" "smoke_tests"
  browser-tests "x86-64" "--mode=opt-host,nacl -k"
}

# These names were originally botnames
# TOOD(robertm): describe what needs to done when those change
# Thunk them to allow non-bots to run without specifying BUILDBOT_GOT_REVISION.
NAME_ARM_OPT() {
  echo "hardy64-marm-narm-opt/rev_${BUILDBOT_GOT_REVISION}"
}
NAME_ARM_DBG() {
  echo "hardy64-marm-narm-dbg/rev_${BUILDBOT_GOT_REVISION}"
}
NAME_ARM_TRY_UPLOAD() {
  echo -n "arm/"
  echo -n "${BUILDBOT_BUILDERNAME}/"
  echo -n "${BUILDBOT_SLAVENAME}/"
  echo -n "${BUILDBOT_BUILDNUMBER}"
}
NAME_ARM_TRY_DOWNLOAD() {
  echo -n "arm/"
  echo -n "${BUILDBOT_TRIGGERED_BY_BUILDERNAME}/"
  echo -n "${BUILDBOT_TRIGGERED_BY_SLAVENAME}/"
  echo -n "${BUILDBOT_TRIGGERED_BY_BUILDNUMBER}"
}

mode-buildbot-arm() {
  FAIL_FAST=false
  local mode=$1

  clobber
  install-lkgr-toolchains

  gyp-arm-build Release

  scons-tests "arm" "${mode} -j8 -k" ""
  scons-tests "arm" "${mode} -k" "small_tests"
  scons-tests "arm" "${mode} -k" "medium_tests"
  scons-tests "arm" "${mode} -k" "large_tests"
  browser-tests "arm" "${mode}"
  ad-hoc-shared-lib-tests "arm"
}

mode-buildbot-arm-dbg() {
  mode-buildbot-arm "--mode=dbg-host,nacl"
  archive-for-hw-bots \
      nativeclient-archive2/between_builders/$(NAME_ARM_DBG)/build.tgz
}

mode-buildbot-arm-opt() {
  mode-buildbot-arm "--mode=opt-host,nacl"
  archive-for-hw-bots \
      nativeclient-archive2/between_builders/$(NAME_ARM_OPT)/build.tgz
}

mode-buildbot-arm-try() {
  mode-buildbot-arm "--mode=opt-host,nacl"
  archive-for-hw-bots \
      nativeclient-trybot/between_builders/$(NAME_ARM_TRY_UPLOAD)/build.tgz
}

mode-buildbot-arm-hw() {
  FAIL_FAST=false
  local flags="naclsdk_validate=0 built_elsewhere=1 $1"
  scons-tests-no-translator "arm" "${flags} -k" "small_tests"
  scons-tests-no-translator "arm" "${flags} -k" "medium_tests"
  scons-tests-no-translator "arm" "${flags} -k" "large_tests"
  browser-tests "arm" "${flags}"
}

# NOTE: the hw bots are too slow to build stuff on so we just
#       use pre-built executables
mode-buildbot-arm-hw-dbg() {
  unarchive-for-hw-bots \
      nativeclient-archive2/between_builders/$(NAME_ARM_DBG)/build.tgz
  mode-buildbot-arm-hw "--mode=dbg-host,nacl"
}

mode-buildbot-arm-hw-opt() {
  unarchive-for-hw-bots \
      nativeclient-archive2/between_builders/$(NAME_ARM_OPT)/build.tgz
  mode-buildbot-arm-hw "--mode=opt-host,nacl"
}

mode-buildbot-arm-hw-try() {
  unarchive-for-hw-bots \
      nativeclient-trybot/between_builders/$(NAME_ARM_TRY_DOWNLOAD)/build.tgz
  mode-buildbot-arm-hw "--mode=opt-host,nacl"
}

# NOTE: clobber and toolchain setup to be done manually, since this is for
# testing a locally built toolchain.
# This runs tests concurrently, so may be more difficult to parse logs.
mode-test-all() {
  local concur=$1

  # turn verbose mode off
  set +o xtrace

  # At least clobber scons-out before building and running the tests though.
  echo "@@@BUILD_STEP clobber@@@"
  rm -rf scons-out

  # First build everything.
  echo "@@@BUILD_STEP scons build @@@"
  scons-tests "arm" "--mode=opt-host,nacl -j${concur}" ""
  scons-tests "x86-32" "--mode=opt-host,nacl -j${concur}" ""
  scons-tests "x86-64" "--mode=opt-host,nacl -j${concur}" ""
  # Then test everything.
  echo "@@@BUILD_STEP scons smoke_tests @@@"
  scons-tests "arm" "--mode=opt-host,nacl -j${concur}" "smoke_tests"
  scons-tests "x86-32" "--mode=opt-host,nacl -j${concur}" "smoke_tests"
  scons-tests "x86-64" "--mode=opt-host,nacl -j${concur}" "smoke_tests"
  # browser tests are run with -j1 on the bots
  browser-tests "arm" "--verbose --mode=opt-host,nacl -j1"
  browser-tests "x86-32" "--verbose --mode=opt-host,nacl -j1"
  browser-tests "x86-64" "--verbose --mode=opt-host,nacl -j1"
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
