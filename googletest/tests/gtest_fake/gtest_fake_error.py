#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Simulate a google-test executable that has an error when listing tests.

http://code.google.com/p/googletest/
"""

import optparse
import sys


def main():
  parser = optparse.OptionParser()
  parser.add_option('--gtest_list_tests', action='store_true')
  parser.add_option('--gtest_filter')
  options, args = parser.parse_args()
  if args:
    parser.error('Failed to process args %s' % args)

  if options.gtest_list_tests:
    sys.stderr.write('Unable to list tests')
    return 1

  sys.stderr.write('Unable to run tests')
  return 1


if __name__ == '__main__':
  sys.exit(main())
