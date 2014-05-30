#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit
set -o xtrace

SCRIPT_DIR="$(cd $(dirname $0) && pwd)"
cd ${SCRIPT_DIR}

OUT_DIR=out
NACLPORTS_URL=http://naclports.googlecode.com/svn/trunk/src
NACLPORTS_REV=1290
NACLPORTS_DIR=${OUT_DIR}/naclports

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

Banner Cloning naclports
if [ ! -d ${NACLPORTS_DIR} ]; then
  mkdir -p ${NACLPORTS_DIR}
  pushd ${NACLPORTS_DIR}
  gclient config --name=src ${NACLPORTS_URL}
  popd
fi

pushd ${NACLPORTS_DIR}
gclient sync -r ${NACLPORTS_REV}
popd


Banner Building lua
pushd ${NACLPORTS_DIR}/src
# Do a 'clean' first, since previous lua build from the naclports bundle
# building might be installed in the toolchain, and that one is built
# without readline support.
make TOOLCHAIN=pnacl clean
make TOOLCHAIN=pnacl lua-ppapi
popd

Banner Done!
