#!/usr/bin/env bash
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

base_dir=$(dirname "$0")

# Update depot_tools unless DEPOT_TOOLS_UPDATE is set to 0
if [ "X$DEPOT_TOOLS_UPDATE" != "X0" ]; then
  if [[ "#grep#fetch#cleanup#diff#setdep#" != *"#$1#"* ]]; then
    "$base_dir"/update_depot_tools "$@"
    case $? in
      123)
        # msys environment was upgraded, need to quit.
        exit 0
        ;;
      0)
        ;;
      *)
        exit $?
    esac
  fi
fi

PYTHONDONTWRITEBYTECODE=1 exec $base_dir/vpython "$base_dir/gclient.py" "$@"
