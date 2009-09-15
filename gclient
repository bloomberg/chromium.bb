#!/bin/bash
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script will try to sync the bootstrap directories and then defer control.

base_dir=$(dirname "$0")

# Update git checkouts prior the cygwin check, we don't want to use msysgit.
if [ "X$DEPOT_TOOLS_UPDATE" != "X0" -a -e "$base_dir/.git" ]
then
  (cd "$base_dir"; git svn rebase -q -q)
fi

if [ "X$DEPOT_TOOLS_UPDATE" != "X0" -a -e "$base_dir/git-cl-repo/.git" ]
then
  (cd "$base_dir/git-cl-repo"; git pull -q)
fi

# Use the batch file as an entry point if on cygwin.
if [ "${OSTYPE}" = "cygwin" -a "${TERM}" != "xterm" ]; then
   ${base_dir}/gclient.bat "$@"
   exit
fi


# We're on POSIX (not cygwin). We can now safely look for svn checkout.
if [ "X$DEPOT_TOOLS_UPDATE" != "X0" -a -e "$base_dir/.svn" ]
then
  # Update the bootstrap directory to stay up-to-date with the latest
  # depot_tools.
  svn -q up "$base_dir"
fi

exec python "$base_dir/gclient.py" "$@"
