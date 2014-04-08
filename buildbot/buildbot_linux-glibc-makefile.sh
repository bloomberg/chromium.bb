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

# Transitionally, even though our new toolchain location is under
# toolchain/linux_x86_nacl_x86/nacl_x86_glibc we have to keep the old format
# inside of the tar (toolchain/linux_x86) so that the untar toolchain script
# is backwards compatible and can untar old tars. Eventually this will be
# unnecessary with the new package_version scheme since how to untar the
# tar file will be embedded inside of the package file so they can differ
# between revisions.
export TOOLCHAINLOC=toolchain
export TOOLCHAINNAME=linux_x86

# This is where we want the toolchain when moving to native_client/toolchain.
OUT_TOOLCHAINLOC=toolchain/linux_x86
OUT_TOOLCHAINNAME=nacl_x86_glibc

TOOL_TOOLCHAIN="${TOOLCHAINLOC}/${TOOLCHAINNAME}"
OUT_TOOLCHAIN="${OUT_TOOLCHAINLOC}/${OUT_TOOLCHAINNAME}"

echo @@@BUILD_STEP gclient_runhooks@@@
gclient runhooks --force

echo @@@BUILD_STEP clobber_toolchain@@@
rm -rf scons-out tools/SRC/*.patch* tools/BUILD/* tools/out tools/toolchain \
  tools/glibc tools/glibc.tar tools/toolchain.t* "${OUT_TOOLCHAIN}" .tmp ||
  echo already_clean

echo @@@BUILD_STEP clean_sources@@@
tools/update_all_repos_to_latest.sh

if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" == "Trybot" ]]; then
echo @@@BUILD_STEP setup source@@@
(cd tools; ./buildbot_patch-toolchain-tries.sh)
fi

echo @@@BUILD_STEP compile_toolchain@@@
(
  cd tools
  make -j8 buildbot-build-with-glibc
  if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" != "Trybot" ]]; then
    make install-glibc INST_GLIBC_PREFIX="$PWD"
  fi
  mkdir -p SRC/newlib/newlib/libc/sys/nacl/include
  cp -aiv ../src/untrusted/pthread/{pthread.h,semaphore.h} \
    SRC/newlib/newlib/libc/sys/nacl/include
  find SRC/newlib/newlib/libc/sys/nacl -name .svn -print0 | xargs -0 rm -rf --
  ( cd SRC/newlib/newlib/libc/sys ; git add nacl )
  for file in SRC/gcc/gcc/configure.ac SRC/gcc/gcc/configure ; do
    cp -aiv $file $file.orig
    sed -e s"|\(CROSS_SYSTEM_HEADER_DIR='\)\(\\\$(gcc_tooldir)/sys-include'\)|\1\$(DESTDIR)\2|" \
      < $file.orig > $file
    touch -r $file.orig $file
    rm $file.orig
    ( cd SRC/gcc/gcc ; git add $(basename $file) )
  done
  make patches
  for patchname in SRC/*.patch ; do
    xz -k -9 "$patchname"
    bzip2 -k -9 "$patchname"
    gzip -9 "$patchname"
    zcat "$patchname".gz > "$patchname"
  done
  mkdir linux
  cp -aiv {SRC/linux-headers-for-nacl/include/,}linux/getcpu.h
  cp -aiv {../src/untrusted/include/machine/,}_default_types.h
  cp -aiv ../LICENSE LICENSE
  mv Makefile Makefile.orig
  . REVISIONS
  sed -e s"|^\\(CANNED_REVISION = \\)no$|\\1$BUILDBOT_GOT_REVISION|" \
      -e s'|^\(SRCDIR =\).*$|\1|' \
      -e s'|\(GCC_CC = \)gcc -m$(HOST_TOOLCHAIN_BITS)|\1gcc|' \
      -e s'|\(GLIBC_CC =.*\)|\1 -I$(abspath $(dir $(THISMAKEFILE)))|' \
      -e s'|\(LINUX_HEADERS = \).*|\1/usr/include|' \
      -e s"|\\(export NACL_FAKE_SONAME\\).*|\\1 = ${NACL_GLIBC_COMMIT:0:8}|" \
         < Makefile.orig > Makefile
  tar czSvpf nacltoolchain-buildscripts-r${BUILDBOT_GOT_REVISION}.tar.gz \
    LICENSE Makefile download_SRC.sh \
    _default_types.h linux newlib-libc-script \
    create_redirector{,s,s_cygwin}.sh redirector{.c,.exe} redirect_table.txt
  rm Makefile
  mv Makefile.orig Makefile
  rm linux/getcpu.h _default_types.h LICENSE
  rmdir linux
)

if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" == "Trybot" ]]; then
  echo @@@BUILD_STEP glibc_tests64@@@
  (
    cd tools
    make glibc-check
  )

  rm -rf "${OUT_TOOLCHAIN}"
  mkdir -p "${OUT_TOOLCHAINLOC}"
  mv "tools/${TOOL_TOOLCHAIN}" "${OUT_TOOLCHAIN}"
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
  wget https://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r"$rev"/glibc_x86.tar.gz -O /dev/null ||
  $GSUTIL cp -a public-read \
    tools/glibc.tgz \
    gs://nativeclient-archive2/between_builders/x86_glibc/r"$rev"/glibc_x86.tar.gz
  echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r"$rev"/@@@

  (
    cd tools
    echo @@@BUILD_STEP sparsify_toolchain@@@
    cp --archive --sparse=always "${TOOL_TOOLCHAIN}" "${TOOL_TOOLCHAIN}_sparse"
    rm -rf "${TOOL_TOOLCHAIN}"
    mv "${TOOL_TOOLCHAIN}_sparse" "${TOOL_TOOLCHAIN}"
    echo @@@BUILD_STEP canonicalize timestamps@@@
    ./canonicalize_timestamps.sh "${TOOL_TOOLCHAIN}"
    echo @@@BUILD_STEP tar_toolchain@@@
    tar Scf toolchain.tar "${TOOL_TOOLCHAIN}"
    xz -k -9 toolchain.tar
    bzip2 -k -9 toolchain.tar
    gzip -n -9 toolchain.tar
    for i in gz bz2 xz ; do
      chmod a+r toolchain.tar.$i
      echo "$(SHA1=$(sha1sum -b toolchain.tar.$i) ; echo ${SHA1:0:40})" \
        > toolchain.tar.$i.sha1hash
    done
  )

  echo @@@BUILD_STEP archive_build@@@
  for suffix in gz gz.sha1hash bz2 bz2.sha1hash xz xz.sha1hash ; do
    $GSUTIL cp -a public-read \
      tools/toolchain.tar.$suffix \
      gs://nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/toolchain_linux_x86.tar.$suffix
  done
  for patch in \
      tools/nacltoolchain-buildscripts-r${BUILDBOT_GOT_REVISION}.tar.gz \
      tools/SRC/*.patch* ; do
    filename="${patch#tools/}"
    filename="${filename#SRC/}"
    $GSUTIL cp -a public-read \
      $patch \
      gs://nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/$filename
  done
  echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/@@@

  echo @@@BUILD_STEP archive_extract_package@@@
  python build/package_version/package_version.py archive \
      --archive-package=nacl_x86_glibc --extract \
      tools/toolchain.tar.bz2,toolchain/linux_x86@https://storage.googleapis.com/nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/toolchain_linux_x86.tar.bz2

  echo @@@BUILD_STEP upload_package_info@@@
  python build/package_version/package_version.py --annotate \
      upload --upload-package=nacl_x86_glibc --revision=${BUILDBOT_GOT_REVISION}

  echo @@@BUILD_STEP glibc_tests64@@@
  (
    cd tools
    make glibc-check
  )
fi

# The script should exit nonzero if any test run fails.
# But that should not short-circuit the script due to the 'set -e' behavior.
exit_status=0
fail() {
  exit_status=1
  return 0
}

# First run 32bit tests, then 64bit tests.  Both should succeed.
export INSIDE_TOOLCHAIN=1
python buildbot/buildbot_standard.py --step-suffix=' (32)' opt 32 glibc || fail
python buildbot/buildbot_standard.py --step-suffix=' (64)' opt 64 glibc || fail

if [[ "${BUILD_COMPATIBLE_TOOLCHAINS:-yes}" != "no" ]]; then
  echo @@@BUILD_STEP sync backports@@@
  rm -rf tools/BACKPORTS/ppapi*
  tools/BACKPORTS/build_backports.sh VERSIONS linux glibc
fi

exit $exit_status
