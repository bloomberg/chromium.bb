#!/bin/bash

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates and runs WebKit layout tests for traditionally
# problematic videos.
# The WebKit LayoutTests test a canonical video in a variety of settings.
# The purpose of this script is to test a variety of videos in one simple
# rendering setting.

# Abort on error.
set -e

SCRIPT_PATH=$(dirname $(realpath $0))
cd $SCRIPT_PATH

# Distance of video_render_tests from src/.
DEPTH=../../..

# Directory names.
TEST_DIR=tests
VIDEO_DIR=videos
LINUX_OUT_DIR=chromium-linux

# Paths relative to video_render_tests.
TEST_PATH=$TEST_DIR
VIDEO_PATH=$TEST_DIR/$VIDEO_DIR
WK_SCRIPT_PATH=$DEPTH/third_party/WebKit/Tools/Scripts
WK_LAYOUTTEST_PATH=$DEPTH/third_party/WebKit/LayoutTests

# Paths relative to third_party/WebKit/LayoutTests.
WK_TESTS_PATH=media/video_render_tests
WK_LINUX_OUT_PATH=platform/$LINUX_OUT_DIR/$WK_TESTS_PATH

LNK_TO_LAYOUT_TESTS=$WK_LAYOUTTEST_PATH/$WK_TESTS_PATH
LNK_TO_EXPECTED_OUT_LINUX=$WK_LAYOUTTEST_PATH/$WK_LINUX_OUT_PATH

# Template and placeholder for video source.
HTML_TEMPLATE=basic-render.html
SOURCE_PLACEHOLDER=%%SOURCE%%

VIDEO_REPO=/cns/cg-d/home/videostack/odd-videos

usage() {
  echo "Usage: $0 [--options]"
  echo "Options:"
  echo "--help: print usage"
  echo "--download: download new files from server"
  exit 0
}

# TODO(vrk): Pass additional arguments to new-run-webkit-tests.
while [[ -n "$1" ]]; do
  case "$1" in
  --download)     download_files=1;;
  --help) usage;;
  esac
  shift
done

# Copies files from the remote directory that do not exist in the local
# directory, into the local directory.
# $1 = the remote directory on CNS (src)
# $2 = the local director (dest)
sync_remote_to_local() {
  if [[ -z "$2" || -n "$3" ]]; then
    echo "Internal error: invalid parameters."
    exit 1
  fi
  remote_repo="$1"
  local_dir="$2"

  # Get file names from remote video repo.
  remote_file_paths=$(fileutil ls $remote_repo)

  if [[ ! -d $local_dir ]]; then
    mkdir -p $local_dir
  fi

  # Download remote files not already in the local directory.
  for file_path in $remote_file_paths; do
    file_name=$(basename $file_path)
    if [[ ! -f $local_dir/$file_name ]]; then
      fileutil cp $file_path $local_dir &
    fi
  done

  wait
}

sync_new_files() {
  echo "Syncing videos..."
  sync_remote_to_local "$VIDEO_REPO/$VIDEO_DIR" "$VIDEO_PATH"
  echo "Syncing expectations..."
  sync_remote_to_local "$VIDEO_REPO/$LINUX_OUT_DIR" "$LINUX_OUT_DIR"
}

# Make sure WebKit stuff is where we expect it to be.
check_webkit_paths() {
  if [[ ! -d $WK_LAYOUTTEST_PATH ]]; then
    echo "Cannot find WebKit Layout Tests directory."
    exit 1;
  fi

  if [[ ! -d $WK_SCRIPT_PATH ]]; then
    echo "Cannot find WebKit scripts directory."
    exit 1;
  fi
}

# Create appropriate link from WebKit to the media directory.
create_links_in_webkit() {
  if [[ ! -L $LNK_TO_LAYOUT_TESTS ]]; then
    ln -s $SCRIPT_PATH/$TEST_PATH $LNK_TO_LAYOUT_TESTS
    echo "Created $LNK_TO_LAYOUT_TESTS"
  fi

  # TODO(vrk): Add support for other OSes.
  if [[ ! -L $LNK_TO_EXPECTED_OUT_LINUX ]]; then
    ln -s $SCRIPT_PATH/$LINUX_OUT_DIR $LNK_TO_EXPECTED_OUT_LINUX
    echo "Created $LNK_TO_EXPECTED_OUT_LINUX"
  fi
}

# Creates HTML layout tests for each of the videos in $VIDEO_PATH
# and puts them in $TEST_PATH.
# Uses $HTML_TEMPLATE as a template.
generate_test_files() {
  echo "Generating test files..."

  if [[ ! -d $VIDEO_PATH || -z $(ls -A $VIDEO_PATH) ]]; then
    echo "No videos in $VIDEO_PATH."
    exit 0;
  fi

  for file in $VIDEO_PATH/*; do
    # Strip directory path to get filename.
    filename=${file#$VIDEO_PATH/}

    # Generate output file path.
    # Change . to - for prettiness.
    output_filename=${filename//./-}.html
    output_path=$TEST_PATH/$output_filename

    # Create replacement source path.
    src_path=$VIDEO_DIR/$filename
    sed -e "s,$SOURCE_PLACEHOLDER,$src_path," $HTML_TEMPLATE > $output_path
    echo "  ...created $output_path"
  done
}

run_layout_tests() {
  echo "Setting up symbolic links..."
  check_webkit_paths
  create_links_in_webkit

  # Run tests.
  echo "Running $WK_TESTS_PATH layout tests."
  $WK_SCRIPT_PATH/new-run-webkit-tests --debug --no-retry -f $WK_TESTS_PATH
}

if [[ "$download_files" = "1" ]]; then
  sync_new_files
  echo
fi

generate_test_files
run_layout_tests
