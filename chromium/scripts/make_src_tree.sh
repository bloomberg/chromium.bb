#!/bin/bash

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script creates a patched FFmpeg tree suitable for using to build
# the FFmpeg needed by chromium.  It takes an |output_dir| where the
# final source tree will reside, and |patches_dir| containing patches
# to apply to the pristine source.
#
# All files inside the directory tree at |patch_dir| matching the pattern
# *.patch will be applied to the output directory in lexicographic order.

set -e

if [ $# -ne 2 ]; then
  echo "Usage: $0 <output directory> <directory with patches>"
  exit 1
fi

output_dir="${1}"
patches_dir="${2}"

if [ ! -d "$patches_dir" ]; then
  echo "$patches_dir is not a directory."
  exit 1
fi

if [ ! -d "$output_dir" ]; then
  echo "$output_dir is not a directory."
  exit 1
fi

# Get a list of all patches in |patches_dir|. Sort them in by the filename
# regardless of path.
reverse_paths() {
  perl -lne "print join '/', reverse split '/'"
}
patches=`find "$patches_dir" -name '*.patch' -type f |
         reverse_paths | sort | reverse_paths`

# Apply the patches.
for p in $patches; do
  echo Apply patch `basename $p` to $output_dir
  patch -p1 -d "$output_dir" < "$p"
done
