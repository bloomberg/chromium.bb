#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Runs a google-test shard.

This makes a simple interface to run a shard on the command line independent of
the interpreter, e.g. cmd.exe vs bash.
"""

import optparse
import os
import subprocess
import sys

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if not ROOT_DIR in sys.path:
  sys.path.insert(0, ROOT_DIR)

from utils import tools


def main():
  tools.disable_buffering()
  parser = optparse.OptionParser(usage='%prog <options> [gtest]')
  parser.disable_interspersed_args()
  parser.add_option(
      '-I', '--index',
      type='int',
      default=os.environ.get('GTEST_SHARD_INDEX'),
      help='Shard index to run')
  parser.add_option(
      '-S', '--shards',
      type='int',
      default=os.environ.get('GTEST_TOTAL_SHARDS'),
      help='Total number of shards to calculate from the --index to run')
  options, args = parser.parse_args()
  env = os.environ.copy()
  env['GTEST_TOTAL_SHARDS'] = str(options.shards)
  env['GTEST_SHARD_INDEX'] = str(options.index)
  return subprocess.call(tools.fix_python_path(args), env=env)


if __name__ == '__main__':
  sys.exit(main())
