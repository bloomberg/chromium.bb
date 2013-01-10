#!/bin/bash
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

ALL_DIRS="
ash/resources
ui/resources
chrome/app/theme
chrome/browser/resources
chrome/renderer/resources
webkit/glue/resources
remoting/resources
remoting/webapp
"

function sanitize_file {
  tput el
  echo -ne "$1\r"
  local file=$1
  local name=$(basename $file)
  pngcrush -d $TMP_DIR -brute -reduce -rem text -rem mkBT \
      -rem mkTS $file > /dev/null
  mv "$TMP_DIR/$name" "$file"
}

function sanitize_dir {
  local dir=$1
  for f in $(find $dir -name "*.png"); do
    sanitize_file $f
  done
}

if [ ! -e ../.gclient ]; then
  echo "$0 must be run in src directory"
  exit 1
fi

# Make sure we have pngcrush installed.
dpkg -s pngcrush > /dev/null 2>&1
if [ "$?" != "0" ]; then
  read -p "Couldn't fnd pngcrush. Do you want to install? (y/n)"
  [ "$REPLY" == "y" ] && sudo apt-get install pngcrush
  [ "$REPLY" == "y" ] || exit
fi

# Create tmp directory for crushed png file.
TMP_DIR=$(mktemp -d)

# Make sure we cleanup temp dir
trap "rm -rf $TMP_DIR" EXIT

# If no arguments passed, sanitize all directories.
DIRS=$*
set ${DIRS:=$ALL_DIRS}

for d in $DIRS; do
  echo "Sanitizing png files in $d"
  sanitize_dir $d
  echo
done

