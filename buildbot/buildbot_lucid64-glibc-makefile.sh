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
  tools/glibc tools/glibc.tar tools/toolchain.t* toolchain .tmp ||
  echo already_clean

echo @@@BUILD_STEP compile_toolchain@@@
(
  cd tools
  make -j8 buildbot-build-with-glibc
  if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" != "Trybot" ]]; then
    make install-glibc INST_GLIBC_PREFIX="$PWD"
  fi
)

if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" == "Trybot" ]]; then
  echo @@@BUILD_STEP glibc_tests64@@@
  (
    cd tools
    make glibc-check
  )

  mkdir -p "$TOOLCHAINLOC"
  rm -rf "$TOOLCHAINLOC/$TOOLCHAINNAME"
  mv {tools/,}"$TOOLCHAINLOC/$TOOLCHAINNAME"
else
  echo @@@BUILD_STEP tar_glibc@@@
  (
    cd tools
    cp --archive --sparse=always glibc glibc_sparse
    rm -rf glibc
    mv glibc_sparse glibc
    cd glibc
    tar zScf ../glibc.tgz ./*
    chmod a+r ../glibc.tgz
  )

  echo @@@BUILD_STEP archive_glibc@@@
  wget http://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r"$(tools/glibc_revision.sh)"/glibc_x86.tar.gz -O /dev/null ||
  /b/build/scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
    tools/glibc.tgz \
    gs://nativeclient-archive2/between_builders/x86_glibc/r"$(tools/glibc_revision.sh)"/glibc_x86.tar.gz
  echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r"$(tools/glibc_revision.sh)"/@@@

  echo @@@BUILD_STEP tar_toolchain@@@
  (
    cd tools
    cp --archive --sparse=always toolchain toolchain_sparse
    rm -rf toolchain
    mv toolchain_sparse toolchain
    tar Scf toolchain.tar toolchain/
    xz -k -9 toolchain.tar
    bzip2 -k -9 toolchain.tar
    gzip -9 toolchain.tar
    chmod a+r toolchain.tar.gz toolchain.tar.bz2 toolchain.tar.xz
  )

  echo @@@BUILD_STEP archive_build@@@
  for suffix in gz bz2 xz; do
    /b/build/scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
      tools/toolchain.tar.$suffix \
      gs://nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/toolchain_linux_x86.tar.$suffix
  done
  echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/@@@

  echo @@@BUILD_STEP untar_toolchain@@@
  (
    mkdir -p .tmp
    cd .tmp
    tar JSxf ../tools/toolchain.tar.xz
    mv toolchain ..
  )

  echo @@@BUILD_STEP glibc_tests64@@@
  (
    cd tools
    make glibc-check
  )
fi

# First run 32bit tests, then 64bit tests. Both shell succeed.
export INSIDE_TOOLCHAIN=1
buildbot/buildbot_linux.sh opt 32 glibc
exec buildbot/buildbot_linux.sh opt 64 glibc
