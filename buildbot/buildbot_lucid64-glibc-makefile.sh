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

GSUTIL=/b/build/scripts/slave/gsutil

this_toolchain="$TOOLCHAINLOC/$TOOLCHAINNAME"

echo @@@BUILD_STEP gclient_runhooks@@@
gclient runhooks --force

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out tools/SRC/* tools/BUILD/* tools/out tools/toolchain \
  tools/glibc tools/glibc.tar tools/toolchain.t* "$this_toolchain" .tmp ||
  echo already_clean

echo @@@BUILD_STEP setup source@@@
(cd tools; ./buildbot_patch-toolchain-tries.sh)

echo @@@BUILD_STEP compile_toolchain@@@
(
  cd tools
  make -j8 buildbot-build-with-glibc
  if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" != "Trybot" ]]; then
    make install-glibc INST_GLIBC_PREFIX="$PWD"
    make pinned-src-newlib
    make patches
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
  rev="$(tools/glibc_revision.sh)"
  wget http://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r"$rev"/glibc_x86.tar.gz -O /dev/null ||
  $GSUTIL -h Cache-Control:no-cache cp -a public-read \
    tools/glibc.tgz \
    gs://nativeclient-archive2/between_builders/x86_glibc/r"$rev"/glibc_x86.tar.gz
  echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r"$rev"/@@@

  echo @@@BUILD_STEP tar_toolchain@@@
  (
    cd tools
    cp --archive --sparse=always "${this_toolchain}" "${this_toolchain}_sparse"
    rm -rf "${this_toolchain}"
    mv "${this_toolchain}_sparse" "${this_toolchain}"
    tar Scf toolchain.tar "${this_toolchain}"
    xz -k -9 toolchain.tar
    bzip2 -k -9 toolchain.tar
    gzip -9 toolchain.tar
    for i in gz bz2 xz ; do
      chmod a+r toolchain.tar.$i
      echo "$(SHA1=$(sha1sum -b toolchain.tar.$i) ; echo ${SHA1:0:40})" \
        > toolchain.tar.$i.sha1hash
    done
  )

  echo @@@BUILD_STEP archive_build@@@
  for suffix in gz gz.sha1hash bz2 bz2.sha1hash xz xz.sha1hash ; do
    $GSUTIL -h Cache-Control:no-cache cp -a public-read \
      tools/toolchain.tar.$suffix \
      gs://nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/toolchain_linux_x86.tar.$suffix
  done
  for patch in tools/SRC/*.patch ; do
    filename="${patch#tools/SRC/}"
    echo $GSUTIL -h Cache-Control:no-cache cp -a public-read \
      $patch \
      gs://nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/$filename
  done
  echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/@@@

  echo @@@BUILD_STEP untar_toolchain@@@
  (
    mkdir -p .tmp
    cd .tmp
    tar JSxf ../tools/toolchain.tar.xz
    mv "${this_toolchain}" ../toolchain
  )

  echo @@@BUILD_STEP glibc_tests64@@@
  (
    cd tools
    make glibc-check
  )
fi

# First run 32bit tests, then 64bit tests.  Both should succeed.
export INSIDE_TOOLCHAIN=1
python buildbot/buildbot_standard.py opt 32 glibc
exec python buildbot/buildbot_standard.py opt 64 glibc
