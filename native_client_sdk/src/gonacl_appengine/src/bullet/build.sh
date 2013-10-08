#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit

if [ -z "${NACL_SDK_ROOT:-}" ]; then
  echo "-------------------------------------------------------------------"
  echo "NACL_SDK_ROOT is unset."
  echo "This environment variable needs to be pointed at some version of"
  echo "the Native Client SDK (the directory containing toolchain/)."
  echo "NOTE: set this to an absolute path."
  echo "-------------------------------------------------------------------"
  exit -1
fi

Banner() {
  echo "######################################################################"
  echo $*
  echo "######################################################################"
}

# echo a command to stdout and then execute it.
LogExecute() {
  echo $*
  $*
}

Clone() {
  local url=$1
  local dir=$2
  if [ ! -d $dir ]; then
    LogExecute git clone $url $dir
  else
    pushd $dir
    LogExecute git pull origin master
    popd
  fi
}

readonly OS_NAME=$(uname -s)
if [ $OS_NAME = "Darwin" ]; then
  OS_JOBS=4
elif [ $OS_NAME = "Linux" ]; then
  OS_JOBS=`nproc`
else
  OS_JOBS=1
fi

NACLPORTS_URL=https://chromium.googlesource.com/external/naclports
NACLPORTS_DIR=naclports
NACLAM_URL=https://github.com/johnmccutchan/NaClAMBase
NACLAM_DIR=NaClAMBase

Banner Cloning naclports
Clone ${NACLPORTS_URL} ${NACLPORTS_DIR}

Banner Building bullet
pushd ${NACLPORTS_DIR}
make NACL_ARCH=pnacl bullet
popd

Banner Cloning NaClAMBase
Clone ${NACLAM_URL} ${NACLAM_DIR}

Banner Building NaClAM
LogExecute cp Makefile ${NACLAM_DIR}
pushd ${NACLAM_DIR}
LogExecute make -j${OS_JOBS}
popd

LogExecute cp ${NACLAM_DIR}/pnacl/Release/NaClAMBullet.{pexe,nmf} .

Banner Done!
