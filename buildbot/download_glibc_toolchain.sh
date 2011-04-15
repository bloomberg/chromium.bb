#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
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

if [ $# -ne 2 ]; then
  echo "USAGE: $0 win/mac/linux 32/64"
  exit 2
fi

set -x
set -e
set -u

PLATFORM=$1
BITS=$2

if [[ ${PLATFORM} == win ]] || [[ ${PLATFORM} == linux ]]; then
  declare -r tar=tar
elif [[ ${PLATFORM} == mac ]]; then
  declare -r tar=gnutar
else
  echo "ERROR, bad platform."
  exit 1
fi

release="$(grep '"x86_toolchain_version":' DEPS |
          (IFS=' "' read prefix toolchain colon release rest ; echo $release))"
rm -rf "toolchain/${PLATFORM}_x86"
TOOLCHAIN_BASE="http://gsdview.appspot.com/nativeclient-archive2/x86_toolchain"
curl --fail --location --url \
    "${TOOLCHAIN_BASE}/r$release/toolchain_${PLATFORM}_x86.tar.gz" \
    -o "toolchain_${PLATFORM}_x86.tar.gz"
$tar xSvpf "toolchain_${PLATFORM}_x86.tar.gz"
rm -f "toolchain_${PLATFORM}_x86.tar.gz"
if [[ $PLATFORM = win ]]; then
  find -L toolchain -type f -xtype l |
  while read name ; do
    # Find gives us some files twice because there are symlinks. Second time
    # 'ln' with fail with ln: 'xxx' and 'yyy' are the same file despite '-f'.
    if [[ -L "$name" ]]; then
      ln -Tf "$(readlink -f "$name")" "$name"
    fi
  done
fi

./scons --nacl_glibc --verbose --mode=nacl_extra_sdk platform=x86-${BITS} \
  extra_sdk_update_header extra_sdk_update
