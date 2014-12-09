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
readonly CORE_SDK=core_sdk
readonly CORE_SDK_WORK=core_sdk_work

export TOOLCHAINLOC=sdk
export TOOLCHAINNAME=nacl-sdk

set -x
set -e
set -u

PLATFORM=$1

cd tools
export INSIDE_TOOLCHAIN=1

echo @@@BUILD_STEP clobber_toolchain@@@
rm -rf ../scons-out \
    sdk-out \
    sdk \
    ${CORE_SDK} \
    ${CORE_SDK_WORK} \
    ../toolchain/${PLATFORM}_x86/nacl_*_newlib \
    BUILD/*

echo @@@BUILD_STEP clean_sources@@@
./update_all_repos_to_latest.sh

if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" == "Trybot" ]]; then
echo @@@BUILD_STEP setup source@@@
./buildbot_patch-toolchain-tries.sh
fi

echo @@@BUILD_STEP compile_toolchain@@@
mkdir -p ../toolchain/${PLATFORM}_x86/nacl_x86_newlib
make -j8 clean buildbot-build-with-newlib

echo @@@BUILD_STEP build_core_sdk@@@
(
  # Use scons to generate the SDK headers and libraries.
  cd ..
  ${NATIVE_PYTHON} scons.py MODE=nacl naclsdk_validate=0 \
    nacl_newlib_dir=tools/sdk/nacl-sdk \
    DESTINATION_ROOT="tools/${CORE_SDK_WORK}" \
    includedir="tools/${CORE_SDK}/x86_64-nacl/include" \
    install_headers

  ${NATIVE_PYTHON} scons.py MODE=nacl naclsdk_validate=0 \
    platform=x86-32 \
    nacl_newlib_dir=tools/sdk/nacl-sdk \
    DESTINATION_ROOT="tools/${CORE_SDK_WORK}" \
    libdir="tools/${CORE_SDK}/x86_64-nacl/lib32" \
    install_lib

  ${NATIVE_PYTHON} scons.py MODE=nacl naclsdk_validate=0 \
    platform=x86-64 \
    nacl_newlib_dir=tools/sdk/nacl-sdk \
    DESTINATION_ROOT="tools/${CORE_SDK_WORK}" \
    libdir="tools/${CORE_SDK}/x86_64-nacl/lib" \
    install_lib
)

echo @@@BUILD_STEP canonicalize timestamps@@@
./canonicalize_timestamps.sh sdk
./canonicalize_timestamps.sh "${CORE_SDK}"

echo @@@BUILD_STEP tar_toolchain@@@
# We don't just use tar's z flag because we want to pass the -n option
# to gzip so that it won't embed a timestamp in the compressed file.
tar cvf - sdk | gzip -n -9 > naclsdk.tgz
tar cvf - "${CORE_SDK}" | gzip -n -9 > core_sdk.tgz

if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" != "Trybot" ]]; then
  GSD_BUCKET=nativeclient-archive2
  UPLOAD_REV=${BUILDBOT_GOT_REVISION}
else
  GSD_BUCKET=nativeclient-trybot/packages
  UPLOAD_REV=${BUILDBOT_BUILDERNAME}/${BUILDBOT_BUILDNUMBER}
fi

# Upload the toolchain before running the tests, in case the tests
# fail.  We do not want a flaky test or a non-toolchain-related bug to
# cause us to lose the toolchain snapshot, especially since this takes
# so long to build on Windows.  We can always re-test a toolchain
# snapshot on the trybots.
echo @@@BUILD_STEP archive_build@@@
(
  gsutil=../buildbot/gsutil.sh
  GS_BASE=gs://${GSD_BUCKET}/toolchain
  for destrevision in ${UPLOAD_REV} latest ; do
    ${gsutil} cp -a public-read \
      naclsdk.tgz \
      ${GS_BASE}/${destrevision}/naclsdk_${PLATFORM}_x86.tgz
    ${gsutil} cp -a public-read \
      core_sdk.tgz \
      ${GS_BASE}/${destrevision}/core_sdk_${PLATFORM}_x86.tgz
  done
)
echo @@@STEP_LINK@download@http://gsdview.appspot.com/${GSD_BUCKET}/toolchain/${UPLOAD_REV}/@@@

if [[ ${PLATFORM} == win ]]; then
  GDB_TGZ=gdb_i686_w64_mingw32.tgz
elif [[ ${PLATFORM} == mac ]]; then
  GDB_TGZ=gdb_x86_64_apple_darwin.tgz
elif [[ ${PLATFORM} == linux ]]; then
  GDB_TGZ=gdb_i686_linux.tgz
else
  echo "ERROR, bad platform."
  exit 1
fi

echo @@@BUILD_STEP archive_extract_package@@@
${NATIVE_PYTHON} ../build/package_version/package_version.py \
  archive --archive-package=${PLATFORM}_x86/nacl_x86_newlib --extract \
  --extra-archive ${GDB_TGZ} \
  naclsdk.tgz,sdk/nacl-sdk@https://storage.googleapis.com/${GSD_BUCKET}/toolchain/${UPLOAD_REV}/naclsdk_${PLATFORM}_x86.tgz \
  core_sdk.tgz,${CORE_SDK}@https://storage.googleapis.com/${GSD_BUCKET}/toolchain/${UPLOAD_REV}/core_sdk_${PLATFORM}_x86.tgz

${NATIVE_PYTHON} ../build/package_version/package_version.py \
  archive --archive-package=${PLATFORM}_x86/nacl_x86_newlib_raw --extract \
  --extra-archive ${GDB_TGZ} \
  naclsdk.tgz,sdk/nacl-sdk@https://storage.googleapis.com/${GSD_BUCKET}/toolchain/${UPLOAD_REV}/naclsdk_${PLATFORM}_x86.tgz

${NATIVE_PYTHON} ../build/package_version/package_version.py \
  --cloud-bucket=${GSD_BUCKET} --annotate \
  upload --skip-missing \
  --upload-package=${PLATFORM}_x86/nacl_x86_newlib --revision=${UPLOAD_REV}

${NATIVE_PYTHON} ../build/package_version/package_version.py \
  --cloud-bucket=${GSD_BUCKET} --annotate \
  upload --skip-missing \
  --upload-package=${PLATFORM}_x86/nacl_x86_newlib_raw --revision=${UPLOAD_REV}

# Before we start testing, put in dummy mock archives so gyp can still untar
# the entire package.
${NATIVE_PYTHON} ../build/package_version/package_version.py fillemptytars \
  --fill-package nacl_x86_newlib

cd ..
if [[ ${PLATFORM} == win ]]; then
  ${NATIVE_PYTHON} buildbot/buildbot_standard.py --scons-args='no_gdb_tests=1' \
    opt 64 newlib
elif [[ ${PLATFORM} == mac ]]; then
  ${NATIVE_PYTHON} buildbot/buildbot_standard.py --scons-args='no_gdb_tests=1' \
    opt 32 newlib
elif [[ ${PLATFORM} == linux ]]; then
  ${NATIVE_PYTHON} buildbot/buildbot_standard.py --scons-args='no_gdb_tests=1' \
    opt 32 newlib
else
  echo "ERROR, bad platform."
  exit 1
fi

# sync_backports is obsolete and should probably be removed.
# if [[ "${DONT_BUILD_COMPATIBLE_TOOLCHAINS:-no}" != "yes" ]]; then
#   echo @@@BUILD_STEP sync backports@@@
#   rm -rf tools/BACKPORTS/ppapi*
#   tools/BACKPORTS/build_backports.sh VERSIONS ${PLATFORM} newlib
# fi
