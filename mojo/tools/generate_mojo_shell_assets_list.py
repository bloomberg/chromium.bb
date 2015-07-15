#!/usr/bin/env python
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Generates the assets_list file from a directory.'''

import argparse
import os
import sys

def main():
  parser = argparse.ArgumentParser(usage='--dir <directory>')

  parser.add_argument('--dir', help='Directory read files from.', required=True)

  options, _ = parser.parse_known_args()

  if not os.path.exists(options.dir) or not os.path.isdir(options.dir):
    print 'Directory does not exist, or path is not a directory', options.dir
    return -1

  root_dir = os.path.abspath(options.dir)
  all_files = []
  for current_root, _, files in os.walk(options.dir):
    current_root_absolute = os.path.abspath(current_root)
    if len(current_root_absolute) < len(root_dir):
      print 'unexpected directory', current_root_absolute
      return -1
    rel_root = current_root_absolute[len(root_dir):]
    if len(rel_root) and rel_root.startswith(os.sep):
      rel_root = rel_root[len(os.sep):]
    if len(rel_root) and not rel_root.endswith(os.sep):
      rel_root += os.sep
    all_files.extend([rel_root + f for f in files])

  with open(os.path.join(options.dir, 'assets_list'), 'w') as f:
    for a_file in all_files:
      f.write(a_file + '\n')

  return 0

if __name__ == '__main__':
  sys.exit(main())

