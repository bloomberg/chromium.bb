#!/usr/bin/env bash
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

base_dir=$(dirname "$0")

if [[ "#grep#fetch#cleanup#diff#" != *"#$1#"* ]]; then
  "$base_dir"/update_depot_tools
fi

PYTHON=python

OUTPUT="$(uname | grep 'CYGWIN')"
CYGWIN=$?
if [ $CYGWIN = 0 ]; then
  PYTHON="$base_dir/python.bat"
fi

PYTHONDONTWRITEBYTECODE=1 exec "$PYTHON" "$base_dir/gclient.py" "$@"
