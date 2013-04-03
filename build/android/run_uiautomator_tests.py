#!/usr/bin/env python
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs both the Python and Java UIAutomator tests."""

import optparse
import os
import sys
import time

from pylib import buildbot_report
from pylib import constants
from pylib import ports
from pylib.base import base_test_result
from pylib.host_driven import run_python_tests
from pylib.instrumentation import dispatch
from pylib.utils import report_results
from pylib.utils import run_tests_helper
from pylib.utils import test_options_parser


def DispatchUIAutomatorTests(options):
  """Dispatches the UIAutomator tests, sharding if possible.

  Uses the logging module to print the combined final results and
  summary of the Java and Python tests. If the java_only option is set, only
  the Java tests run. If the python_only option is set, only the python tests
  run. If neither are set, run both Java and Python tests.

  Args:
    options: command-line options for running the Java and Python tests.

  Returns:
    An integer representing the number of broken tests.
  """
  if not options.keep_test_server_ports:
    # Reset the test port allocation. It's important to do it before starting
    # to dispatch any tests.
    if not ports.ResetTestServerPortAllocation():
      raise Exception('Failed to reset test server port.')

  all_results = base_test_result.TestRunResults()

  if options.run_java_tests:
    all_results.AddTestRunResults(dispatch.Dispatch(options))
  if options.run_python_tests:
    all_results.AddTestRunResults(run_python_tests.DispatchPythonTests(options))

  report_results.LogFull(
      results=all_results,
      test_type='UIAutomator',
      test_package=os.path.basename(options.test_jar),
      annotation=options.annotation,
      build_type=options.build_type,
      flakiness_server=options.flakiness_dashboard_server)

  return len(all_results.GetNotPass())


def main(argv):
  option_parser = optparse.OptionParser()
  test_options_parser.AddUIAutomatorOptions(option_parser)
  options, args = option_parser.parse_args(argv)
  test_options_parser.ValidateUIAutomatorOptions(option_parser, options, args)

  run_tests_helper.SetLogLevel(options.verbose_count)
  ret = 1
  try:
    ret = DispatchUIAutomatorTests(options)
  finally:
    buildbot_report.PrintStepResultIfNeeded(options, ret)
  return ret


if __name__ == '__main__':
  sys.exit(main(sys.argv))
