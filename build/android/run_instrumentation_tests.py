#!/usr/bin/env python
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs both the Python and Java tests."""

import optparse
import os
import sys
import time

from pylib import buildbot_report
from pylib import constants
from pylib import ports
from pylib.host_driven import run_python_tests
from pylib.instrumentation import apk_info
from pylib.instrumentation import run_java_tests
from pylib.test_result import TestResults
from pylib.utils import run_tests_helper
from pylib.utils import test_options_parser


def DispatchInstrumentationTests(options):
  """Dispatches the Java and Python instrumentation tests, sharding if possible.

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

  start_date = int(time.time() * 1000)
  java_results = TestResults()
  python_results = TestResults()

  if options.run_java_tests:
    java_results = run_java_tests.DispatchJavaTests(
        options,
        [apk_info.ApkInfo(options.test_apk_path, options.test_apk_jar_path)])
  if options.run_python_tests:
    python_results = run_python_tests.DispatchPythonTests(options)

  all_results = TestResults.FromTestResults([java_results, python_results])

  all_results.LogFull(
      test_type='Instrumentation',
      test_package=options.test_apk,
      annotation=options.annotation,
      build_type=options.build_type,
      flakiness_server=options.flakiness_dashboard_server)

  return len(all_results.GetAllBroken())


def main(argv):
  option_parser = optparse.OptionParser()
  test_options_parser.AddInstrumentationOptions(option_parser)
  options, args = option_parser.parse_args(argv)
  test_options_parser.ValidateInstrumentationOptions(option_parser, options,
                                                     args)

  run_tests_helper.SetLogLevel(options.verbose_count)
  buildbot_report.PrintNamedStep(
      'Instrumentation tests: %s - %s' % (', '.join(options.annotation),
                                          options.test_apk))
  ret = 1
  try:
    ret = DispatchInstrumentationTests(options)
  finally:
    buildbot_report.PrintStepResultIfNeeded(options, ret)
  return ret


if __name__ == '__main__':
  sys.exit(main(sys.argv))
