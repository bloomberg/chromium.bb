#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Simulate a passing google-test executable that return 1.

The exit code is considered the primary reliable indicator of test success. So
even if all the test cases printed [       OK ], it doesn't mean the test run is
valid if the exit code is non-zero. So run_test_cases.py must retry all the test
cases individually to figure out which test case failed.

http://code.google.com/p/googletest/
"""

import sys

import gtest_fake_base


TESTS = {
  'Foo': ['Bar1'],
}


def main():
  test_cases, _ = gtest_fake_base.parse_args(TESTS, 0)

  for test_case in test_cases:
    print gtest_fake_base.get_test_output(test_case, False)
  print gtest_fake_base.get_footer(len(test_cases), len(test_cases))
  return 1


if __name__ == '__main__':
  sys.exit(main())
