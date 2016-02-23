#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import sys


def DoMain(argv):
  parser = argparse.ArgumentParser(description='Generate forwarding headers.')
  parser.add_argument('-i', '--list-inputs', action='store_true',
                      help='List input files and exit.')
  parser.add_argument('-o', '--list-outputs', action='store_true',
                      help='List output files and exit.')
  parser.add_argument('-d', '--dest-dir', type=str,
                      help=('Output directory for forwarding headers.'))
  parser.add_argument('filenames', metavar='filename', type=str, nargs='+',
                      help='Input filenames.')

  args = parser.parse_args(argv)
  if args.list_inputs:
    return list_inputs(args.filenames)

  if not args.dest_dir:
    print '--dest-dir is required for this command.'
    sys.exit(1)
  if args.list_outputs:
    return ' '.join(
        os.path.join(args.dest_dir, os.path.basename(filename))
        for filename in args.filenames)

  if not os.path.isdir(args.dest_dir):
    os.makedirs(args.dest_dir)
  for filename in args.filenames:
    target_filename = os.path.join(args.dest_dir, os.path.basename(filename))
    if os.path.isfile(target_filename):
      os.unlink(target_filename)
    try:
      os.link(filename, target_filename)
    except OSError as e:
      # Fallbacks to copy if hardlinking fails.
      shutil.copy(filename, target_filename)


if __name__ == '__main__':
  results = DoMain(sys.argv[1:])
  if results:
    print results
