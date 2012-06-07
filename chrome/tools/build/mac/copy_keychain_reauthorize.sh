#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A normal 'copies' section slightly alters the binaries copied by changing
# the vmsize of their __LINKEDIT segments, invalidating their signatures. This
# script performs the copies without alteration.

set -eu

if [[ ${#} -lt 2 ]]; then
  echo "usage: ${0} <target_dir> file [...]" >& 2
  exit 1
fi

TARGET_DIR="${1}"
shift

mkdir -p "${TARGET_DIR}"
cp "${@}" "${TARGET_DIR}"
