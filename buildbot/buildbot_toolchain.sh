#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
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
rm -rf ../scons-out sdk-out sdk ../toolchain/*_newlib BUILD/*

echo @@@BUILD_STEP clean_sources@@@
./update_all_repos_to_latest.sh

if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" == "Trybot" ]]; then
echo @@@BUILD_STEP setup source@@@
./buildbot_patch-toolchain-tries.sh
fi

echo @@@BUILD_STEP compile_toolchain@@@
mkdir -p ../toolchain/${PLATFORM}_x86
make -j8 clean buildbot-build-with-newlib
if [[ ${PLATFORM} == win ]]; then
../mingw/msys/bin/sh.exe -c "export PATH=/mingw/bin:/bin:\$PATH &&
  export TOOLCHAINLOC &&
  export TOOLCHAINNAME &&
  make -j8 gdb 2>&1"
fi

echo @@@BUILD_STEP canonicalize timestamps@@@
./canonicalize_timestamps.sh sdk

echo @@@BUILD_STEP tar_toolchain@@@
# We don't just use tar's z flag because we want to pass the -n option
# to gzip so that it won't embed a timestamp in the compressed file.
tar cvf - sdk | gzip -n -9 > naclsdk.tgz
chmod a+r naclsdk.tgz
if [ "$PLATFORM" = "mac" ] ; then
  echo "$(SHA1=$(openssl sha1 naclsdk.tgz) ; echo ${SHA1/* /})"
else
  echo "$(SHA1=$(sha1sum -b naclsdk.tgz) ; echo ${SHA1:0:40})"
fi > naclsdk.tgz.sha1hash

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
        ${gsutil} cp -a public-read \
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

if [[ "${DONT_BUILD_COMPATIBLE_TOOLCHAINS:-no}" != "yes" ]]; then
  echo @@@BUILD_STEP sync backports@@@
  rm -rf tools/BACKPORTS/ppapi*
  tools/BACKPORTS/build_backports.sh VERSIONS ${PLATFORM} newlib
fi
