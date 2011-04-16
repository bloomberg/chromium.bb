#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
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
export TOOLCHAINNAME=linux_x86

echo @@@BUILD_STEP gclient_runhooks@@@
gclient runhooks --force

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out tools/SRC tools/BUILD tools/out tools/toolchain \
  tools/glibc tools/glibc.tar tools/toolchain.tgz toolchain .tmp ||
  echo already_clean

echo @@@BUILD_STEP compile_toolchain@@@
(
  cd tools
  make -j8 buildbot-mark-version pinned-src-glibc pinned-src-linux-headers-for-nacl
  perl -pi -e s'|ppl_minor_version=10|ppl_minor_version=11|' SRC/{binutils,gcc}/configure{,.ac}
  find SRC -print0 | xargs -0 touch -r SRC
  make -j8 build-with-glibc
  make install-glibc INST_GLIBC_PREFIX="$PWD"
)

echo @@@BUILD_STEP tar_glibc@@@
(
  cd tools/glibc
  tar zScf ../glibc.tgz ./*
  chmod a+r ../glibc.tgz
)

echo @@@BUILD_STEP tar_toolchain@@@
(
  cd tools
  tar zScf toolchain.tgz toolchain/
  chmod a+r toolchain.tgz
)

if [[ "$BUILDBOT_SLAVE_TYPE" != "Trybot" ]]; then
  echo @@@BUILD_STEP archive_glibc@@@
  wget http://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r"$(tools/glibc_revision.sh)"/glibc_x86.tar.gz -O /dev/null ||
  /b/build/scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
    tools/glibc.tgz \
    gs://nativeclient-archive2/between_builders/x86_glibc/r"$(tools/glibc_revision.sh)"/glibc_x86.tar.gz
  echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r"$(tools/glibc_revision.sh)"/@@@

  echo @@@BUILD_STEP archive_build@@@
  /b/build/scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
    tools/toolchain.tgz \
    gs://nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/toolchain_linux_x86.tar.gz
  echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/@@@
fi

echo @@@BUILD_STEP untar_toolchain@@@
(
  mkdir -p .tmp
  cd .tmp
  tar zSxf ../tools/toolchain.tgz
  mv toolchain ..
)

# First run 32bit tests, then 64bit tests. Both shell succeed.
export INSIDE_TOOLCHAIN=1
buildbot/buildbot_linux.sh opt 32 glibc
exec buildbot/buildbot_linux.sh opt 32 glibc
