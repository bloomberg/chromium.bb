#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script assumed to be run in native_client/
if [ "x${OSTYPE}" = "xcygwin" ]; then
  cd "$(cygpath "${PWD}")"
fi
if [[ ${PWD} != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

if [ $# -ne 1 ]; then
  echo "USAGE: $0 win/mac/linux"
  exit 2
fi

readonly SCRIPT_DIR="$(dirname "$0")"
readonly SCRIPT_DIR_ABS="$(cd "${SCRIPT_DIR}" ; pwd)"

export TOOLCHAINLOC=sdk
export TOOLCHAINNAME=nacl-sdk

set -x
set -e
set -u

PLATFORM=$1

cd tools
export INSIDE_TOOLCHAIN=1

echo @@@BUILD_STEP clobber_toolchain@@@
rm -rf ../scons-out sdk-out sdk ../toolchain SRC/* BUILD/*

echo @@@BUILD_STEP compile_toolchain@@@
mkdir -p ../toolchain/${PLATFORM}_x86
make -j8 clean buildbot-build-with-newlib

echo @@@BUILD_STEP tar_toolchain@@@
tar cvfz naclsdk.tgz sdk/
chmod a+r naclsdk.tgz

if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" != "Trybot" ]]; then
  # Upload the toolchain before running the tests, in case the tests
  # fail.  We do not want a flaky test or a non-toolchain-related bug to
  # cause us to lose the toolchain snapshot, especially since this takes
  # so long to build on Windows.  We can always re-test a toolchain
  # snapshot on the trybots.
  echo @@@BUILD_STEP archive_build@@@
  ( # Use the batch file as an entry point if on cygwin.
    if [ "x${OSTYPE}" = "xcygwin" ]; then
      # Use extended globbing (cygwin should always have it).
      shopt -s extglob
      # Filter out cygwin python (everything under /usr or /bin, or *cygwin*).
      export PATH=${PATH/#\/bin*([^:])/}
      export PATH=${PATH//:\/bin*([^:])/}
      export PATH=${PATH/#\/usr*([^:])/}
      export PATH=${PATH//:\/usr*([^:])/}
      export PATH=${PATH/#*([^:])cygwin*([^:])/}
      export PATH=${PATH//:*([^:])cygwin*([^:])/}
      gsutil="${SCRIPT_DIR_ABS}/../../../../../scripts/slave/gsutil.bat"
    else
      gsutil=/b/build/scripts/slave/gsutil
    fi
    GS_BASE=gs://nativeclient-archive2/toolchain
    "$gsutil" -h Cache-Control:no-cache cp -a public-read \
      naclsdk.tgz \
      ${GS_BASE}/${BUILDBOT_GOT_REVISION}/naclsdk_${PLATFORM}_x86.tgz
    "$gsutil" -h Cache-Control:no-cache cp -a public-read \
      naclsdk.tgz \
      ${GS_BASE}/latest/naclsdk_${PLATFORM}_x86.tgz
  )
  echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/toolchain/${BUILDBOT_GOT_REVISION}/@@@
fi

echo @@@BUILD_STEP untar_toolchain@@@
mkdir -p ../toolchain/${PLATFORM}_x86_newlib/.tmp
cd ../toolchain/${PLATFORM}_x86_newlib/.tmp
tar xfz ../../../tools/naclsdk.tgz
mv sdk/nacl-sdk/* ../

cd ../../..


if [[ ${PLATFORM} == win ]]; then
  cmd /c "call buildbot\\buildbot_win.bat opt 64 newlib"
elif [[ ${PLATFORM} == mac ]]; then
  buildbot/buildbot_mac.sh opt 32 newlib
elif [[ ${PLATFORM} == linux ]]; then
  buildbot/buildbot_linux.sh opt 32 newlib
else
  echo "ERROR, bad platform."
  exit 1
fi
