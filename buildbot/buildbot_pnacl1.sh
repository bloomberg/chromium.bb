#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o xtrace
set -o nounset
set -o errexit

clobber() {
  echo "@@@BUILD_STEP clobber@@@"
  rm -rf scons-out toolchain compiler hg ../xcodebuild ../sconsbuild ../out \
    src/third_party/nacl_sdk/arm-newlib
}

# This is the first thing you want to run on the bots to install the toolchains
install-lkgr-toolchains() {
  echo "@@@BUILD_STEP install_toolchains@@@"
  gclient runhooks --force
}

# We usually do not trust the TC to provide the latest (extra) SDK
# so we rebuild them here
# NOTE: we split this in two stages so that we can use -j8 for the second
partial-sdk() {
  local platforms=$1
  # TODO(robertm): teach utman to only build the sdk for those platforms
  #                we care about
  echo "@@@BUILD_STEP partial_sdk@@@"
  UTMAN_BUILDBOT=true tools/llvm/utman.sh extrasdk-make-install
}

# copy data to well known archive server
push-data-to-archive-server() {
  /b/build/scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
    $1 \
    gs://nativeclient-archive2/$2
}

# These tars up the executable to be shipped to the arm HW bots
archive-for-hw-bots() {
  local target=$1
  echo "@@@BUILD_STEP tar_generated_binaries@@@"
  tar cvfz arm.tgz scons-out/

  echo "@@@BUILD_STEP archive_binaries@@@"
  push-data-to-archive-server arm.tgz ${target}
}

# Build with gyp - this only exercises the trusted TC and hence this only
# makes sense to run for ARM.
gyp-arm-build() {
  gypmode=$1
  # Setup environment for arm.
  export TOOLCHAIN_DIR=native_client/toolchain/linux_arm-trusted/arm-2009q3
  export TOOLCHAIN_BIN=${TOOLCHAIN_DIR}/bin
  export AR=${TOOLCHAIN_BIN}/arm-none-linux-gnueabi-ar
  export AS=${TOOLCHAIN_BIN}/arm-none-linux-gnueabi-as
  export CC=${TOOLCHAIN_BIN}/arm-none-linux-gnueabi-gcc
  export CXX=${TOOLCHAIN_BIN}/arm-none-linux-gnueabi-g++
  export LD=${TOOLCHAIN_BIN}/arm-none-linux-gnueabi-ld
  export RANLIB=${TOOLCHAIN_BIN}/arm-none-linux-gnueabi-ranlib
  export SYSROOT
  export GYP_DEFINES="target_arch=arm \
    sysroot=${TOOLCHAIN_DIR}/arm-none-linux-gnueabi/libc \
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

  echo "@@@BUILD_STEP gyp_tests [${gypmode}]@@@"
  python trusted_test.py --config ${gypmode}
}


ad-hoc-shared-lib-tests() {
  # TODO(robertm): make this accessible by the utman script so that this get
  # http://code.google.com/p/nativeclient/issues/detail?id=1647
  echo "@@@BUILD_STEP fake_shared_libs@@@"
  pushd  tests/pnacl_ld_example/
  make -f Makefile.pnacl clean
  make -f Makefile.pnacl preparation
  make -f Makefile.pnacl run.x86-32
  make -f Makefile.pnacl run.arm
  popd
}

readonly SCONS_COMMON="./scons --verbose bitcode=1 -j8 -k"

single-scons-test() {
  local platform=$1
  local extra=$2
  # this extra is actually printed as part of the build step
  local extra2=$3
  local test=$4
  echo "@@@BUILD_STEP scons [${platform}] [${test}] [${extra2}]@@@"
  ${SCONS_COMMON} ${extra} ${extra2} platform=${platform} ${test} || \
     (RETCODE=$? && echo @@@STEP_FAILURE@@@)


}

scons-tests() {
  local platforms=$1
  local extra=$2
  local test=$3
  for platform in ${platforms} ; do
    single-scons-test ${platform} "${extra}" "" "${test}"
    single-scons-test ${platform} "${extra}" "nacl_pic=1" "${test}"
    if [ "${platform}" != "arm" ] ; then
      single-scons-test ${platform} "${extra}" \
        "use_sandboxed_translator=1" "${test}"
    fi
  done
}

######################################################################
mode-trybot() {
  clobber
  install-lkgr-toolchains
  partial-sdk "arm x86-32 x86-64"
  scons-tests "arm x86-32 x86-64" "--mode=opt-host,nacl" "smoke_tests"
  ad-hoc-shared-lib-tests
}

mode-buildbot-x8632() {
  clobber
  install-lkgr-toolchains
  partial-sdk "x86-32"
  # First build everything
  scons-tests "x86-32" "--mode=opt-host,nacl" ""
  # Then test (not all nexes which are build are also tested)
  scons-tests "x86-32" "--mode=opt-host,nacl" "smoke_tests"
  # this really tests arm and x86-32
  ad-hoc-shared-lib-tests
}

mode-buildbot-x8664() {
  clobber
  install-lkgr-toolchains
  partial-sdk "x86-64"
  # First build everything
  scons-tests "x86-64" "--mode=opt-host,nacl" ""
  # Then test (not all nexes which are build are also tested)
  scons-tests "x86-64" "--mode=opt-host,nacl" "smoke_tests"
}

# For arm we also test the trusted cross toolchain, hence we
# we use opt and dbg flavors and run gyp tests.
# NOTE: that those do not affect the nexe's
mode-buildbot-arm-opt() {
  clobber
  install-lkgr-toolchains
  partial-sdk "arm"

  gyp-arm-build Release

  scons-tests "arm" "--mode=opt-host,nacl" ""
  scons-tests "arm" "--mode=opt-host,nacl" "small_tests"
  scons-tests "arm" "--mode=opt-host,nacl" "medium_tests"
  scons-tests "arm" "--mode=opt-host,nacl" "large_tests"
  archive-for-hw-bots \
    between_builders/${BUILDBOT_BUILDERNAME}/rev_${BUILDBOT_GOT_REVISION}/build.tgz
}

mode-buildbot-arm-dbg() {
  clobber
  install-lkgr-toolchains
  partial-sdk "arm"

  gyp-arm-build Debug

  scons-tests "arm" "--mode=dbg-host,nacl" ""
  scons-tests "arm" "--mode=dbg-host,nacl" "small_tests"
  scons-tests "arm" "--mode=dbg-host,nacl" "medium_tests"
  scons-tests "arm" "--mode=dbg-host,nacl" "large_tests"
  archive-for-hw-bots \
    between_builders/${BUILDBOT_BUILDERNAME}/rev_${BUILDBOT_GOT_REVISION}/build.tgz

}

# NOTE: clobber and toolchain setup to be done manually
mode-utman() {
  # turn verbose mode off
  set +o xtrace
  scons-tests "arm x86-32 x86-64" "--mode=opt-host,nacl" "smoke_tests"
  ad-hoc-shared-lib-tests
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

eval "$@"

