#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

if [ $# -ne 0 ]; then
  echo "USAGE: $0"
  exit 2
fi

set -x
set -e
set -u

RETCODE=0

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out toolchain compiler hg ../xcodebuild ../sconsbuild ../out \
    src/third_party/nacl_sdk/arm-newlib
rm -rf ../toolchain ../hg

echo @@@BUILD_STEP compile_toolchain@@@
UTMAN_DEBUG=true tools/llvm/utman.sh download-trusted
UTMAN_DEBUG=true tools/llvm/utman.sh untrusted_sdk arm-untrusted.tgz
chmod a+r arm-untrusted.tgz

echo @@@BUILD_STEP untar_toolchain@@@
# Untar to ensure we can and to place the toolchain where the main build
# expects it to be.
mkdir -p toolchain/linux_arm-untrusted
cd toolchain/linux_arm-untrusted
tar xfz ../../arm-untrusted.tgz
cd ../..

echo @@@BUILD_STEP test-arm@@@
UTMAN_DEBUG=true tools/llvm/utman.sh test-arm \
    (RETCODE=$? && echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP test-arm-pic@@@
UTMAN_DEBUG=true tools/llvm/utman.sh test-arm-pic \
    (RETCODE=$? && echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP test-x86-32@@@
UTMAN_DEBUG=true tools/llvm/utman.sh test-x86-32 \
    (RETCODE=$? && echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP test-x86-32-pic@@@
UTMAN_DEBUG=true tools/llvm/utman.sh test-x86-32-pic \
    (RETCODE=$? && echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP test-x86-64@@@
UTMAN_DEBUG=true tools/llvm/utman.sh test-x86-64 \
    (RETCODE=$? && echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP test-x86-64-pic@@@
UTMAN_DEBUG=true tools/llvm/utman.sh test-x86-64-pic \
    (RETCODE=$? && echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP archive_build@@@
if [[ ${BUILDBOT_BUILDERNAME} == lucid32-toolchain_arm-untrusted ]]; then
  SUFFIX=
else
  SUFFIX=-${BUILDBOT_BUILDERNAME}
fi
GS_BASE=gs://nativeclient-archive2/toolchain
../../../scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
    native_client/arm-untrusted.tgz \
    ${GS_BASE}/${BUILDBOT_GOT_REVISION}/naclsdk_linux_arm-untrusted${SUFFIX}.tgz

exit ${RETCODE}
