#!/bin/sh
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will try to sync the root and bootstrap directories.

base_dir=$(dirname "$0")

# Skip if $DEPOT_TOOLS_UPDATE==0 or ../.svn/. doesn't exist.
if [ "X$DEPOT_TOOLS_UPDATE" != "X0" -a -e "$base_dir/../.svn" ]
then
  # Update the root directory.
  svn -q up "$base_dir/.."
fi

exec python "$base_dir/../gclient.py" "$@"
