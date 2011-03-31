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

if [[ ${PLATFORM} == win ]]; then
  cmd /c "call buildbot\\buildbot_win.bat opt 32"
elif [[ ${PLATFORM} == mac ]]; then
  buildbot/buildbot_${PLATFORM}.sh opt
elif [[ ${PLATFORM} == linux ]]; then
  buildbot/buildbot_${PLATFORM}.sh opt 32
else
  echo "ERROR, bad platform."
  exit 1
fi

echo @@@BUILD_STEP archive_build@@@
GS_BASE=gs://nativeclient-archive2/toolchain
/b/build/scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
    native_client/tools/naclsdk.tgz \
    ${GS_BASE}/${BUILDBOT_GOT_REVISION}/naclsdk_${PLATFORM}_x86.tgz
