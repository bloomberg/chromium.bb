#!/usr/bin/env python
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script removes the ThinLTO cache if:
 * the stampfile for this script does not exist.
 * the state of |arm64_allow_outlining| in gn.args has changed.

Added as part of crbug.com/931297
"""

import argparse
import os
import shutil
import sys

def check_cache_exists_and_clear(cache_location):
  if os.path.exists(cache_location):
    shutil.rmtree(cache_location)

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--cache-location')
  parser.add_argument('--stampfile')
  parser.add_argument('--allowing-outlining', action='store_true')
  args = parser.parse_args()

  if not os.path.exists(args.stampfile):
    # If we've never checked before, clear the ThinLTO cache.
    check_cache_exists_and_clear(args.cache_location)

  else:
    # If we've checked before, check if outlining has been toggled and clear
    # the ThinLTO cache if so.
    with open(args.stampfile, 'r') as handle:
      previously_allowed_outlining = handle.read().strip() == 'True'

      if previously_allowed_outlining != args.allowing_outlining:
        check_cache_exists_and_clear(args.cache_location)

  with open(args.stampfile, 'w') as handle:
    handle.write(str(args.allowing_outlining))

  return 0

if __name__ == '__main__':
  sys.exit(main())
