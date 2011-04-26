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


echo @@@BUILD_STEP extract_archive@@@
if [[ $BUILDBOT_SLAVENAME == nacl-arm2 ]]; then
  PARENT_BUILDER=nacl-arm_${MODE}
  VERSION=None
else
  PARENT_BUILDER=hardy64-marm-narm-${MODE}
  VERSION=rev_${BUILDBOT_GOT_REVISION}
fi
curl -L \
    http://commondatastorage.googleapis.com/nativeclient-archive2/\
between_builders/${PARENT_BUILDER}/${VERSION}/build.tgz \
    -o build.tgz
tar xvfz build.tgz --no-same-owner

echo @@@BUILD_STEP small_tests@@@
ARM_CC=gcc ARM_CXX=g++ ARM_LIB_DIR=/usr/lib \
    ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    --mode=${MODE}-linux,nacl small_tests platform=arm bitcode=1 \
    sdl=none naclsdk_validate=0 built_elsewhere=1 ||
    { RETCODE=$? && echo @@@STEP_FAILURE@@@;}

echo @@@BUILD_STEP medium_tests@@@
ARM_CC=gcc ARM_CXX=g++ ARM_LIB_DIR=/usr/lib \
    ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    --mode=${MODE}-linux,nacl medium_tests platform=arm bitcode=1 \
    sdl=none naclsdk_validate=0 built_elsewhere=1 ||
    { RETCODE=$? && echo @@@STEP_FAILURE@@@;}

echo @@@BUILD_STEP large_tests@@@
ARM_CC=gcc ARM_CXX=g++ ARM_LIB_DIR=/usr/lib \
    ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    --mode=${MODE}-linux,nacl large_tests platform=arm bitcode=1 \
    sdl=none naclsdk_validate=0 built_elsewhere=1 ||
    { RETCODE=$? && echo @@@STEP_FAILURE@@@;}

echo @@@BUILD_STEP chrome_browser_tests@@@
# Although we could use the "browser_headless=1" Scons option, it runs
# xvfb-run once per Chromium invocation.  This is good for isolating
# the tests, but xvfb-run has a stupid fixed-period sleep, which would
# slow down the tests unnecessarily.
XVFB_PREFIX="xvfb-run --auto-servernum"

ARM_CC=gcc ARM_CXX=g++ ARM_LIB_DIR=/usr/lib \
    $XVFB_PREFIX \
    ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    --mode=${MODE}-linux,nacl SILENT=1 platform=arm bitcode=1 sdl=none \
    built_elsewhere=1 naclsdk_mode=manual naclsdk_validate=0 \
    chrome_browser_tests ||
    { RETCODE=$? && echo @@@STEP_FAILURE@@@;}

# TODO(mseaborn): Drop support for non-IRT builds so that this is the
# default.  See http://code.google.com/p/nativeclient/issues/detail?id=1691
echo @@@BUILD_STEP chrome_browser_tests using IRT@@@
ARM_CC=gcc ARM_CXX=g++ ARM_LIB_DIR=/usr/lib \
    $XVFB_PREFIX \
    ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    --mode=${MODE}-linux,nacl SILENT=1 platform=arm bitcode=1 sdl=none \
    built_elsewhere=1 naclsdk_mode=manual naclsdk_validate=0 \
    chrome_browser_tests irt=1 ||
    { RETCODE=$? && echo @@@STEP_FAILURE@@@;}

echo @@@BUILD_STEP pyauto_tests@@@
ARM_CC=gcc ARM_CXX=g++ ARM_LIB_DIR=/usr/lib \
    $XVFB_PREFIX \
    ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    --mode=${MODE}-linux,nacl SILENT=1 platform=arm bitcode=1 sdl=none \
    built_elsewhere=1 naclsdk_mode=manual naclsdk_validate=0 \
    pyauto_tests ||
    { RETCODE=$? && echo @@@STEP_FAILURE@@@;}


exit ${RETCODE}
