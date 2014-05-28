#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Delete files in directories matching a pattern.
"""

import glob
import optparse
import os
import sys

from util import build_utils

def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '--pattern',
      help='Pattern for matching Files to delete.')
  parser.add_option(
      '--keep',
      help='Files to keep even if they matches the pattern.')
  parser.add_option(
      '--stamp',
      help='Path to touch on success')

  options, args = parser.parse_args()

  if not options.pattern or not args:
    print 'No --pattern or target directories given'
    return

  for target_dir in args:
    target_pattern = os.path.join(target_dir, options.pattern)
    matching_files = glob.glob(target_pattern)

    keep_pattern = os.path.join(target_dir, options.keep)
    files_to_keep = glob.glob(keep_pattern)

    for target_file in matching_files:
      if target_file in files_to_keep:
        continue

      if os.path.isfile(target_file):
        os.remove(target_file)

  if options.stamp:
    build_utils.Touch(options.stamp)

if __name__ == '__main__':
  sys.exit(main())

