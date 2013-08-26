#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script assumed to be run in native_client/
if [[ ${PWD} != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

set -x
set -e
set -u

export TOOLCHAINLOC=toolchain
export TOOLCHAINNAME=mac_x86
export INST_GLIBC_PROGRAM="$PWD/tools/glibc_download.sh"

this_toolchain="$TOOLCHAINLOC/$TOOLCHAINNAME"

GSUTIL=/b/build/scripts/slave/gsutil

echo @@@BUILD_STEP gclient_runhooks@@@
gclient runhooks --force

echo @@@BUILD_STEP clobber_toolchain@@@
rm -rf scons-out tools/BUILD/* tools/out/* tools/toolchain \
  tools/glibc tools/glibc.tar tools/toolchain.t* "${this_toolchain}" .tmp ||
  echo already_clean
mkdir -p tools/toolchain/mac_x86

echo @@@BUILD_STEP clean_sources@@@
tools/update_all_repos_to_latest.sh

# glibc_download.sh can return three return codes:
#  0 - glibc is successfully downloaded and installed
#  1 - glibc is not downloaded but another run may help
#  2+ - glibc is not downloaded and can not be downloaded later
#
# If the error result is 2 or more we are stopping the build
echo @@@BUILD_STEP check_glibc_revision_sanity@@@
echo "Try to download glibc revision $(tools/glibc_revision.sh)"
if tools/glibc_download.sh tools/toolchain/mac_x86 1; then
  INST_GLIBC_PROGRAM=true
elif (($?>1)); then
  echo @@@STEP_FAILURE@@@
  exit 100
fi

if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" == "Trybot" ]]; then
echo @@@BUILD_STEP setup source@@@
(cd tools; ./buildbot_patch-toolchain-tries.sh)
fi

echo @@@BUILD_STEP compile_toolchain@@@
(
  cd tools
  make -j8 buildbot-build-with-glibc
)

if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" == "Trybot" ]]; then
  mkdir -p "$TOOLCHAINLOC"
  rm -rf "$TOOLCHAINLOC/$TOOLCHAINNAME"
  mv {tools/,}"$TOOLCHAINLOC/$TOOLCHAINNAME"
else
  (
    cd tools
    echo @@@BUILD_STEP canonicalize timestamps@@@
    ./canonicalize_timestamps.sh "${this_toolchain}"
    echo @@@BUILD_STEP tar_toolchain@@@
    tar Scf toolchain.tar "${this_toolchain}"
    bzip2 -k -9 toolchain.tar
    gzip -n -9 toolchain.tar
    for i in gz bz2 ; do
      chmod a+x toolchain.tar.$i
      echo "$(SHA1=$(openssl sha1 toolchain.tar.$i) ; echo ${SHA1/* /})" \
        > toolchain.tar.$i.sha1hash
    done
  )

  echo @@@BUILD_STEP archive_build@@@
  for suffix in gz gz.sha1hash bz2 bz2.sha1hash ; do
    $GSUTIL cp -a public-read \
      tools/toolchain.tar.$suffix \
      gs://nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/toolchain_mac_x86.tar.$suffix
  done
  echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/@@@

  echo @@@BUILD_STEP untar_toolchain@@@
  (
    mkdir -p .tmp
    cd .tmp
    # GNU tar does not like some headers, regular tar can not create sparse files.
    # Use regular tar with non-sparse files for now.
    tar zxf ../tools/toolchain.tar.gz
    mv "${this_toolchain}" ../toolchain
  )
fi

# The script should exit nonzero if any test run fails.
# But that should not short-circuit the script due to the 'set -e' behavior.
exit_status=0
fail() {
  exit_status=1
  return 0
}

export INSIDE_TOOLCHAIN=1
python buildbot/buildbot_standard.py opt 32 glibc || fail

if [[ "${BUILD_COMPATIBLE_TOOLCHAINS:-yes}" != "no" ]]; then
  echo @@@BUILD_STEP sync backports@@@
  rm -rf tools/BACKPORTS/ppapi*
  tools/BACKPORTS/build_backports.sh VERSIONS mac glibc
fi

exit $exit_status
