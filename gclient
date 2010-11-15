#!/usr/bin/env bash
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

base_dir=$(dirname "$0")

# Use the batch file as an entry point if on cygwin.  Needs to happen before
# the call to update the tools or the update will happen twice.
if [ "${OSTYPE}" = "cygwin" -a "${TERM}" != "xterm" ]; then
   ${base_dir}/gclient.bat "$@"
   exit
fi

"$base_dir"/update_depot_tools

exec python "$base_dir/gclient.py" "$@"
