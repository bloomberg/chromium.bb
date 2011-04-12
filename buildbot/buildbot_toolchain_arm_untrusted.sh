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

if [[ ${BUILDBOT_BUILDERNAME} == lucid32-toolchain_arm-untrusted ]]; then
  # Don't test arm + 64-bit on 32-bit builder.
  # We can't build 64-bit trusted components on a 32-bit system.
  # Arm disabled on 32-bit because it runs out of memory.
  TOOLCHAIN_LABEL=pnacl_linux_i686
  RUN_TESTS="x86-32 x86-32-pic"
else
  TOOLCHAIN_LABEL=pnacl_linux_x86_64
  RUN_TESTS="x86-32 x86-32-pic arm arm-pic x86-64 x86-64-pic"
fi

RETCODE=0

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out toolchain compiler hg ../xcodebuild ../sconsbuild ../out \
    src/third_party/nacl_sdk/arm-newlib
rm -rf ../toolchain ../hg

echo @@@BUILD_STEP show-config@@@
UTMAN_BUILDBOT=true tools/llvm/utman.sh show-config

echo @@@BUILD_STEP compile_toolchain@@@
UTMAN_BUILDBOT=true tools/llvm/utman.sh download-trusted
UTMAN_BUILDBOT=true tools/llvm/utman.sh untrusted_sdk pnacl-toolchain.tgz
chmod a+r pnacl-toolchain.tgz

echo @@@BUILD_STEP untar_toolchain@@@
# Untar to ensure we can and to place the toolchain where the main build
# expects it to be.
mkdir -p toolchain/${TOOLCHAIN_LABEL}
cd toolchain/${TOOLCHAIN_LABEL}
tar xfz ../../pnacl-toolchain.tgz
cd ../..

echo @@@BUILD_STEP archive_build@@@
GS_BASE=gs://nativeclient-archive2/toolchain
/b/build/scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
    pnacl-toolchain.tgz \
    ${GS_BASE}/${BUILDBOT_GOT_REVISION}/naclsdk_${TOOLCHAIN_LABEL}.tgz
/b/build/scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
    pnacl-toolchain.tgz \
    ${GS_BASE}/latest/naclsdk_${TOOLCHAIN_LABEL}.tgz

for arch in ${RUN_TESTS} ; do
  echo @@@BUILD_STEP test-${arch}@@@
  UTMAN_BUILDBOT=true tools/llvm/utman.sh test-${arch} ||
      (RETCODE=$? && echo @@@STEP_FAILURE@@@)
done

exit ${RETCODE}
