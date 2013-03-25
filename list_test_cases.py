#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""List all the test cases for a google test.

See more info at http://code.google.com/p/googletest/.
"""

import os
import sys

import run_test_cases


def main():
  """CLI frontend to validate arguments."""
  run_test_cases.run_isolated.disable_buffering()
  parser = run_test_cases.OptionParserWithTestShardingAndFiltering(
      usage='%prog <options> [gtest]')
  options, args = parser.parse_args()
  if not args:
    parser.error('Please provide the executable to run')

  cmd = run_test_cases.fix_python_path(args)
  try:
    tests = run_test_cases.list_test_cases(
        cmd,
        os.getcwd(),
        index=options.index,
        shards=options.shards,
        disabled=options.disabled,
        fails=options.fails,
        flaky=options.flaky,
        pre=options.pre,
        manual=options.manual,
        seed=0)
    for test in tests:
      print test
  except run_test_cases.Failure, e:
    print e.args[0]
    return e.args[1]
  return 0


if __name__ == '__main__':
  sys.exit(main())
