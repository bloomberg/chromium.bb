#!/bin/sh
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will try to sync the bootstrap directories and then defer control.

base_dir=$(dirname "$0")
 
# Use the batch file as an entry point if on cygwin.
if [ "${OSTYPE}" = "cygwin" -a "${TERM}" = "cygwin" ]; then
   ${base_dir}/gclient.bat "$@"
   exit
fi


if [ "X$DEPOT_TOOLS_UPDATE" != "X0" -a -e "$base_dir/.svn" ]
then
  # Update the bootstrap directory to stay up-to-date with the latest
  # depot_tools.
  svn -q up "$base_dir/bootstrap"

  # Then defer the control to the bootstrapper.
  exec "$base_dir/bootstrap/gclient.sh" "$@"
else
  exec python "$base_dir/gclient.py" "$@"
fi
