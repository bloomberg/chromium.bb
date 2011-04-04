#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

if [ $# -ne 1 ]; then
  echo "USAGE: $0 dbg/opt"
  exit 2
fi

set -x
set -e
set -u

# Pick dbg or opt
MODE=$1

RETCODE=0

if [[ $MODE == dbg ]]; then
  GYPMODE=Debug
else
  GYPMODE=Release
fi

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


echo @@@BUILD_STEP gclient_runhooks@@@
gclient runhooks --force

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out toolchain compiler hg ../xcodebuild ../sconsbuild ../out \
    src/third_party/nacl_sdk/arm-newlib

echo @@@BUILD_STEP partial_sdk@@@
./scons --verbose --download --mode=nacl_extra_sdk platform=arm bitcode=1 \
    sdl=none extra_sdk_clean extra_sdk_update_header install_libpthread \
    extra_sdk_update

echo @@@BUILD_STEP gyp_compile@@@
make -C .. -k -j4 V=1 BUILDTYPE=${GYPMODE}

echo @@@BUILD_STEP gyp_tests@@@
python trusted_test.py --config ${GYPMODE}

echo @@@BUILD_STEP scons_compile@@@
./scons -j 8 DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    --mode=${MODE}-linux,nacl,doc platform=arm bitcode=1 sdl=none
tar cvfz arm.tgz scons-out/

echo @@@BUILD_STEP small_tests@@@
./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    --mode=${MODE}-linux,nacl,doc small_tests platform=arm bitcode=1 \
    sdl=none ||
    (RETCODE=$? && echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP medium_tests@@@
./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    --mode=${MODE}-linux,nacl,doc medium_tests platform=arm bitcode=1 \
    sdl=none ||
    (RETCODE=$? && echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP large_tests@@@
./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    --mode=${MODE}-linux,nacl,doc large_tests platform=arm bitcode=1 \
    sdl=none ||
    (RETCODE=$? && echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP archive_build@@@
if [[ $BUILDBOT_BUILDERNAME == nacl-* ]]; then
  VERSION=None
else
  VERSION=rev_${BUILDBOT_GOT_REVISION}
fi
/b/build/scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
    arm.tgz gs://nativeclient-archive2/between_builders/\
${BUILDBOT_BUILDERNAME}/${VERSION}/build.tgz

exit ${RETCODE}
