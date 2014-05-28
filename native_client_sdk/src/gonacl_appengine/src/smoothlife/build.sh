#!/bin/bash
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit

SCRIPT_DIR="$(cd $(dirname $0) && pwd)"
cd ${SCRIPT_DIR}

OUT_DIR=out
SMOOTHLIFE_URL=https://github.com/binji/smoothnacl
SMOOTHLIFE_DIR=${OUT_DIR}/smoothlife
SMOOTHLIFE_SHA=e81cf2b00290add556dcdc8aa84506ca007bc853

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
  local sha=$3
  if [ ! -d $dir ]; then
    LogExecute git clone $url $dir
  else
    pushd $dir
    LogExecute git fetch origin
    popd
  fi

  pushd $dir
  LogExecute git checkout $sha
  popd
}

readonly OS_NAME=$(uname -s)
if [ $OS_NAME = "Darwin" ]; then
  OS_JOBS=4
elif [ $OS_NAME = "Linux" ]; then
  OS_JOBS=`nproc`
else
  OS_JOBS=1
fi

Banner Cloning smoothlife
Clone ${SMOOTHLIFE_URL} ${SMOOTHLIFE_DIR} ${SMOOTHLIFE_SHA}

pushd ${SMOOTHLIFE_DIR}

Banner Updating submodules
LogExecute git submodule update --init

Banner Building FFTW
LogExecute make ports TOOLCHAIN=pnacl CONFIG=Release

Banner Building smoothlife
LogExecute make TOOLCHAIN=pnacl CONFIG=Release -j${OS_JOBS}

popd

LogExecute cp ${SMOOTHLIFE_DIR}/pnacl/Release/smoothnacl.{pexe,nmf} ${OUT_DIR}

Banner Done!
