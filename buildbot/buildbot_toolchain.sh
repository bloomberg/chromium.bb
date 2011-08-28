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
echo "$(SHA1="$(sha1sum -b "naclsdk.tgz")" ; echo "${SHA1:0:40}")" \
  > naclsdk.tgz.sha1hash

if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" != "Trybot" ]]; then
  # Upload the toolchain before running the tests, in case the tests
  # fail.  We do not want a flaky test or a non-toolchain-related bug to
  # cause us to lose the toolchain snapshot, especially since this takes
  # so long to build on Windows.  We can always re-test a toolchain
  # snapshot on the trybots.
  echo @@@BUILD_STEP archive_build@@@
  (
    gsutil=../buildbot/gsutil.sh
    GS_BASE=gs://nativeclient-archive2/toolchain
    for destrevision in ${BUILDBOT_GOT_REVISION} latest ; do
      for suffix in tgz tgz.sha1hash ; do
        ${gsutil} -h Cache-Control:no-cache cp -a public-read \
          naclsdk.${suffix} \
          ${GS_BASE}/${destrevision}/naclsdk_${PLATFORM}_x86.${suffix}
      done
    done
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
  # Explicitly call the depot tools version of Python to avoid cygwin issues.
  python.bat buildbot/buildbot_standard.py opt 64 newlib
elif [[ ${PLATFORM} == mac ]]; then
  python buildbot/buildbot_standard.py opt 32 newlib
elif [[ ${PLATFORM} == linux ]]; then
  python buildbot/buildbot_standard.py opt 32 newlib
else
  echo "ERROR, bad platform."
  exit 1
fi
