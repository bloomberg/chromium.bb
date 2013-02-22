#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs content browser tests."""

import optparse
import sys

from pylib.browsertests import dispatch
from pylib.utils import run_tests_helper
from pylib.utils import test_options_parser

def main(argv):
  option_parser = optparse.OptionParser()
  test_options_parser.AddGTestOptions(option_parser)
  options, args = option_parser.parse_args(argv)

  if len(args) > 1:
    option_parser.error('Unknown argument: %s' % args[1:])

  run_tests_helper.SetLogLevel(options.verbose_count)
  return dispatch.Dispatch(options)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
