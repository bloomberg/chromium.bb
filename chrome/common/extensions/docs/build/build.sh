#!/bin/sh

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

BUILD_DIR=$(dirname "$0")

function depot_tools_error() {
  echo "Cannot find depot_tools python - is it installed and in your path?" 1>&2
  exit 1
}

if [ "$(uname | cut -b1-6)" == "CYGWIN" ] ; then
  # On cygwin, we use the verison of python from depot_tools.
  echo "Detected cygwin - looking for python in depot_tools"
  GCLIENT_PATH=$(which gclient)
  if ! [ -f "$GCLIENT_PATH" ] ; then
    depot_tools_error
  fi
  DEPOT_TOOLS=$(dirname "$GCLIENT_PATH")
  PYTHON_PATH="$DEPOT_TOOLS/python"
  if ! [ -f "$PYTHON_PATH" ] ; then
    depot_tools_error
  fi

  # The output from build.py doesn't seem seem to print to the console until
  # it's finished, so print a message so people don't think it's hung.
  echo "Running - this can take about a minute"
  echo "(it goes faster if you have a Release build of DumpRenderTree)"

  $PYTHON_PATH $BUILD_DIR/build.py $*
else
  # On all other platforms, we just run the script directly.
  $BUILD_DIR/build.py $*
fi
