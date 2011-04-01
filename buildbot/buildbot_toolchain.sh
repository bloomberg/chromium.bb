#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script assumed to be run in native_client/
cd "$(cygpath "${PWD}")"
if [[ ${PWD} != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

if [ $# -ne 1 ]; then
  echo "USAGE: $0 win/mac/linux"
  exit 2
fi

set -x
set -e
set -u

PLATFORM=$1

cd tools
export INSIDE_TOOLCHAIN=1


echo @@@BUILD_STEP clobber_toolchain@@@
rm -rf ../scons-out sdk-out sdk ../toolchain SRC BUILD

echo @@@BUILD_STEP compile_toolchain@@@
mkdir -p ../toolchain/${PLATFORM}_x86
make clean build SDKLOC=`pwd`/sdk GCC_VERSION=4.4.3 CROSSARCH=nacl64

echo @@@BUILD_STEP tar_toolchain@@@
tar cvfz naclsdk.tgz sdk/
chmod a+r naclsdk.tgz

echo @@@BUILD_STEP untar_toolchain@@@
mkdir -p ../toolchain/${PLATFORM}_x86/.tmp
cd ../toolchain/${PLATFORM}_x86/.tmp
tar xfz ../../../tools/naclsdk.tgz
mv sdk/nacl-sdk/* ../

cd ../../..

echo @@@BUILD_STEP archive_build@@@
# Upload the toolchain before running the tests, in case the tests
# fail.  We do not want a flaky test or a non-toolchain-related bug to
# cause us to lose the toolchain snapshot, especially since this takes
# so long to build on Windows.  We can always re-test a toolchain
# snapshot on the trybots.
GS_BASE=gs://nativeclient-archive2/toolchain
/b/build/scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
    tools/naclsdk.tgz \
    ${GS_BASE}/${BUILDBOT_GOT_REVISION}/naclsdk_${PLATFORM}_x86.tgz
/b/build/scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
    tools/naclsdk.tgz \
    ${GS_BASE}/latest/naclsdk_${PLATFORM}_x86.tgz

if [[ ${PLATFORM} == win ]]; then
  cmd /c "call buildbot\\buildbot_win.bat opt 64"
elif [[ ${PLATFORM} == mac ]]; then
  buildbot/buildbot_${PLATFORM}.sh opt
elif [[ ${PLATFORM} == linux ]]; then
  buildbot/buildbot_${PLATFORM}.sh opt 32
else
  echo "ERROR, bad platform."
  exit 1
fi
