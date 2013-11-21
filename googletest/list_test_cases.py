#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""List all the test cases for a google test.

See more info at http://code.google.com/p/googletest/.
"""

import os
import sys

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if not ROOT_DIR in sys.path:
  sys.path.insert(0, ROOT_DIR)

from googletest import run_test_cases
from utils import tools


def main():
  """CLI frontend to validate arguments."""
  tools.disable_buffering()
  parser = run_test_cases.OptionParserWithTestShardingAndFiltering(
      usage='%prog <options> [gtest]')

  # Override default seed value to default to 0.
  parser.set_defaults(seed=0)

  options, args = parser.parse_args()
  if not args:
    parser.error('Please provide the executable to run')

  cmd = tools.fix_python_path(args)
  try:
    tests = run_test_cases.chromium_list_test_cases(
        cmd,
        os.getcwd(),
        index=options.index,
        shards=options.shards,
        seed=options.seed,
        disabled=options.disabled,
        fails=options.fails,
        flaky=options.flaky,
        pre=False,
        manual=options.manual)
    for test in tests:
      print test
  except run_test_cases.Failure, e:
    print e.args[0]
    return e.args[1]
  return 0


if __name__ == '__main__':
  sys.exit(main())
