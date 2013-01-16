#!/bin/bash
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The optimization code is based on pngslim (http://goo.gl/a0XHg)
# and executes a similar pipleline to optimize the png file size.
# The steps that require pngoptimizercl/pngrewrite/deflopt are omitted,
# but this runs all other processes, including:
# 1) various color-dependent optimizations using optipng.
# 2) optimize the number of huffman blocks.
# 3) randomize the huffman table.
# 4) Further optimize using optipng and advdef (zlib stream).
# Due to the step 3), each run may produce slightly different results.
#
# Note(oshima): In my experiment, advdef didn't reduce much. I'm keeping it
# for now as it does not take much time to run.

readonly ALL_DIRS="
ash/resources
ui/resources
chrome/app/theme
chrome/browser/resources
chrome/renderer/resources
webkit/glue/resources
remoting/resources
remoting/webapp
"

# Constants used for optimization
readonly MIN_BLOCK_SIZE=128
readonly LIMIT_BLOCKS=256
readonly RANDOM_TRIALS=100

# Global variables for stats
TOTAL_OLD_BYTES=0
TOTAL_NEW_BYTES=0
TOTAL_FILE=0
PROCESSED_FILE=0

declare -a THROBBER_STR=('-' '\\' '|' '/')
THROBBER_COUNT=0

# Show throbber character at current cursor position.
function throbber {
  echo -ne "${THROBBER_STR[$THROBBER_COUNT]}\b"
  let THROBBER_COUNT=($THROBBER_COUNT+1)%4
}

# Usage: pngout_loop <file> <png_out_options> ...
# Optimize the png file using pngout with the given options
# using various block split thresholds and filter types.
function pngout_loop {
  local file=$1
  shift
  local opts=$*
  for i in 0 128 256 512; do
    for j in $(seq 0 5); do
      throbber
      pngout -q -k1 -s1 -b$i -f$j $opts $file
    done
  done
}

# Usage: process_grayscale <file>
# Optimize grayscale images for all color bit depths.
#
# TODO(oshima): Experiment with -d0 w/o -c0.
function process_grayscale {
  echo -n "|gray"
  for opt in -d1 -d2 -d4 -d8; do
    pngout_loop $file -c0 $opt
  done
}

# Usage: process_grayscale_alpha <file>
# Optimize grayscale images with alpha for all color bit depths.
function process_grayscale_alpha {
  echo -n "|gray-a"
  pngout_loop $file -c4
  for opt in -d1 -d2 -d4 -d8; do
    pngout_loop $file -c3 $opt
  done
}

# Usage: process_rgb <file>
# Optimize rgb images with or without alpha for all color bit depths.
function process_rgb {
  echo -n "|rgb"
  for opt in -d1 -d2 -d4 -d8; do
    pngout_loop $file -c3 $opt
  done
  pngout_loop $file -c2
  pngout_loop $file -c6
}

# Usage: huffman_blocks <file>
# Optimize the huffman blocks.
function huffman_blocks {
  local file=$1
  echo -n "|huffman"
  local size=$(stat -c%s $file)
  let MAX_BLOCKS=$size/$MIN_BLOCK_SIZE
  if [ $MAX_BLOCKS -gt $LIMIT_BLOCKS ]; then
    MAX_BLOCKS=$LIMIT_BLOCKS
  fi
  for i in $(seq 2 $MAX_BLOCKS); do
    throbber
    pngout -q -k1 -ks -s1 -n$i $file
  done
}

# Usage: random_huffman_table_trial <file>
# Try compressing by randomizing the initial huffman table.
#
# TODO(oshima): Try adjusting different parameters for large files to
# reduce runtime.
function random_huffman_table_trial {
  echo -n "|random"
  local file=$1
  local old_size=$(stat -c%s $file)
  for i in $(seq 1 $RANDOM_TRIALS); do
    throbber
    pngout -q -k1 -ks -s0 -r $file
  done
  local new_size=$(stat -c%s $file)
  if [ $new_size -lt $old_size ]; then
    random_huffman_table_trial $file
  fi
}

# Usage: final_comprssion <file>
# Further compress using optipng and advdef.
# TODO(oshima): Experiment with 256.
function final_compression {
  echo -n "|final"
  local file=$1
  for i in 32k 16k 8k 4k 2k 1k 512; do
    throbber
    optipng -q -nb -nc -zw$i -zc1-9 -zm1-9 -zs0-3 -f0-5 $file
  done
  for i in $(seq 1 4); do
    throbber
    advdef -q -z -$i $file
  done
  echo -ne "\r"
}

# Usage: optimize_size <file>
# Performs png file optimization.
function optimize_size {
  tput el
  local file=$1
  echo -n "$file "

  advdef -q -z -4 $file

  pngout -q -s4 -c0 -force $file $file.tmp.png
  if [ -f $file.tmp.png ]; then
    rm $file.tmp.png
    process_grayscale $file
    process_grayscale_alpha $file
  else
    pngout -q -s4 -c4 -force $file $file.tmp.png
    if [ -f $file.tmp.png ]; then
      rm $file.tmp.png
      process_grayscale_alpha $file
    else
      process_rgb $file
    fi
  fi

  echo -n "|filter"
  optipng -q -zc9 -zm8 -zs0-3 -f0-5 $file
  pngout -q -k1 -s1 $file

  huffman_blocks $file

  # TODO(oshima): Experiment with strategy 1.
  echo -n "|strategy"
  for i in 3 2 0; do
    pngout -q -k1 -ks -s$i $file
  done

  random_huffman_table_trial $file

  final_compression $file
}

# Usage: process_file <file>
function process_file {
  local file=$1
  local name=$(basename $file)
  # -rem alla removes all ancillary chunks except for tRNS
  pngcrush -d $TMP_DIR -brute -reduce -rem alla $file > /dev/null

  if [ ! -z "$OPTIMIZE" ]; then
    optimize_size $TMP_DIR/$name
  fi
}

# Usage: sanitize_file <file>
function sanitize_file {
  local file=$1
  local name=$(basename $file)
  local old=$(stat -c%s $file)
  local tmp_file=$TMP_DIR/$name

  process_file $file

  local new=$(stat -c%s $tmp_file)
  let diff=$old-$new
  let TOTAL_OLD_BYTES+=$old
  let TOTAL_NEW_BYTES+=$new
  let percent=($diff*100)/$old
  let TOTAL_FILE+=1

  tput el
  if [ $new -lt $old ]; then
    echo -ne "$file : $old => $new ($diff bytes : $percent %)\n"
    mv "$tmp_file" "$file"
    let PROCESSED_FILE+=1
  else
    if [ -z "$OPTIMIZE" ]; then
      echo -ne "$file : skipped\r"
    fi
    rm $tmp_file
  fi
}

function sanitize_dir {
  local dir=$1
  for f in $(find $dir -name "*.png"); do
    sanitize_file $f
  done
}

function install_if_not_installed {
  local program=$1
  dpkg -s $program > /dev/null 2>&1
  if [ "$?" != "0" ]; then
    read -p "Couldn't find $program. Do you want to install? (y/n)"
    [ "$REPLY" == "y" ] && sudo apt-get install $program
    [ "$REPLY" == "y" ] || exit
  fi
}

function fail_if_not_installed {
  local program=$1
  local url=$2
  which $program > /dev/null
  if [ $? != 0 ]; then
    echo "Couldn't find $program. Please download and install it from $url"
    exit 1
  fi
}

function show_help {
  local program=$(basename $0)
  echo \
"Usage: $program [options] dir ...

$program is a utility to reduce the size of png files by removing
unnecessary chunks and compressing the image.

Options:
  -o  Aggressively optimize file size. Warning: this is *VERY* slow and
      can take hours to process all files.
  -h  Print this help text."
  exit 1
}

if [ ! -e ../.gclient ]; then
  echo "$0 must be run in src directory"
  exit 1
fi

# Parse options
while getopts oh opts
do
  case $opts in
    o)
      OPTIMIZE=true;
      shift;;
    [h?])
      show_help;;
  esac
done

# Make sure we have all necessary commands installed.
install_if_not_installed pngcrush
if [ ! -z "$OPTIMIZE" ]; then
  install_if_not_installed optipng
  fail_if_not_installed advdef "http://advancemame.sourceforge.net/comp-download.html"
  fail_if_not_installed pngout "http://www.jonof.id.au/kenutils"
fi

# Create tmp directory for crushed png file.
TMP_DIR=$(mktemp -d)

# Make sure we cleanup temp dir
trap "rm -rf $TMP_DIR" EXIT

# If no directories are specified, sanitize all directories.
DIRS=$@
set ${DIRS:=$ALL_DIRS}

for d in $DIRS; do
  echo "Sanitizing png files in $d"
  sanitize_dir $d
  echo
done

# Print the results.
let diff=$TOTAL_OLD_BYTES-$TOTAL_NEW_BYTES
let percent=$diff*100/$TOTAL_OLD_BYTES
echo "Processed $PROCESSED_FILE files (out of $TOTAL_FILE files)" \
     "in $(date -u -d @$SECONDS +%T)s"
echo "Result : $TOTAL_OLD_BYTES => $TOTAL_NEW_BYTES bytes" \
     "($diff bytes : $percent %)"

