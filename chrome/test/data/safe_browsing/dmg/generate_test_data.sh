#!/bin/sh

# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

THIS_DIR=$(dirname "$0")

OUT_DIR="$1"

if [[ ! "$1" ]]; then
  echo "Usage: $(basename "$0") [output_dir]"
  exit 1
fi

if [[ -e "$1" && ! -d "$1" ]]; then
  echo "Output directory \`$1' exists but is not a directory."
  exit 1
fi
if [[ ! -d "$1" ]]; then
  mkdir -p "$1"
fi

generate_test_data() {
  # HFS Raw Images #############################################################

  MAKE_HFS="${THIS_DIR}/make_hfs.sh"
  "${MAKE_HFS}" HFS+ 1024 "${OUT_DIR}/hfs_plus.img"
  "${MAKE_HFS}" hfsx $((8 * 1024)) "${OUT_DIR}/hfsx_case_sensitive.img"
}

# Silence any log output.
generate_test_data &> /dev/null
