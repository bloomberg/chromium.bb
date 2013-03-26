#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the Monkey tests on one or more devices."""
import logging
import optparse
import random
import sys

from pylib import android_commands
from pylib.base import base_test_result
from pylib.host_driven import python_test_base
from pylib.host_driven import python_test_sharder
from pylib.utils import report_results
from pylib.utils import test_options_parser


class MonkeyTest(python_test_base.PythonTestBase):
  def testMonkey(self):
    # Launch and wait for Chrome to launch.
    self.adb.StartActivity(self.options.package_name,
                           self.options.activity_name,
                           wait_for_completion=True,
                           action='android.intent.action.MAIN',
                           force_stop=True)

    # Chrome crashes are not always caught by Monkey test runner.
    # Verify Chrome has the same PID before and after the test.
    before_pids = self.adb.ExtractPid(self.options.package_name)

    # Run the test.
    output = ''
    if before_pids:
      output = '\n'.join(self._LaunchMonkeyTest())
      after_pids = self.adb.ExtractPid(self.options.package_name)

    crashed = (not before_pids or not after_pids
               or after_pids[0] != before_pids[0])

    results = base_test_result.TestRunResults()
    if 'Monkey finished' in output and not crashed:
      result = base_test_result.BaseTestResult(
          self.qualified_name, base_test_result.ResultType.PASS, log=output)
    else:
      result = base_test_result.BaseTestResult(
          self.qualified_name, base_test_result.ResultType.FAIL, log=output)
    results.AddResult(result)
    return results

  def _LaunchMonkeyTest(self):
    """Runs monkey test for a given package.

    Looks at the following parameters in the options object provided
    in class initializer:
      package_name: Allowed package.
      category: A list of allowed categories.
      throttle: Delay between events (ms).
      seed: Seed value for pseduo-random generator. Same seed value
        generates the same sequence of events. Seed is randomized by
        default.
      event_count: Number of events to generate.
      verbosity: Verbosity level [0-3].
      extra_args: A string of other args to pass to the command verbatim.
    """

    category = self.options.category or []
    seed = self.options.seed or random.randint(1, 100)
    throttle = self.options.throttle or 100
    event_count = self.options.event_count or 10000
    verbosity = self.options.verbosity or 1
    extra_args = self.options.extra_args or ''

    timeout_ms = event_count * throttle * 1.5

    cmd = ['monkey',
           '-p %s' % self.options.package_name,
           ' '.join(['-c %s' % c for c in category]),
           '--throttle %d' % throttle,
           '-s %d' % seed,
           '-v ' * verbosity,
           '--monitor-native-crashes',
           '--kill-process-after-error',
           extra_args,
           '%d' % event_count]
    return self.adb.RunShellCommand(' '.join(cmd), timeout_time=timeout_ms)


def DispatchPythonTests(options):
  """Dispatches the Monkey tests, sharding it if there multiple devices."""
  logger = logging.getLogger()
  logger.setLevel(logging.DEBUG)
  attached_devices = android_commands.GetAttachedDevices()
  if not attached_devices:
    raise Exception('You have no devices attached or visible!')

  # Actually run the tests.
  logging.debug('Running monkey tests.')
  # TODO(frankf): This is a stop-gap solution. Come up with a
  # general way for running tests on every devices.
  available_tests = []
  for k in range(len(attached_devices)):
    new_method = 'testMonkey%d' % k
    setattr(MonkeyTest, new_method, MonkeyTest.testMonkey)
    available_tests.append(MonkeyTest(new_method))
  options.ensure_value('shard_retries', 1)
  sharder = python_test_sharder.PythonTestSharder(
      attached_devices, available_tests, options)
  results = sharder.RunShardedTests()
  report_results.LogFull(
      results=results,
      test_type='Monkey',
      test_package='Monkey',
      build_type=options.build_type)
  report_results.PrintAnnotation(results)


def main():
  desc = 'Run the Monkey tests on 1 or more devices.'
  parser = optparse.OptionParser(description=desc)
  test_options_parser.AddBuildTypeOption(parser)
  parser.add_option('--package-name', help='Allowed package.')
  parser.add_option('--activity-name',
                    default='com.google.android.apps.chrome.Main',
                    help='Name of the activity to start [default: %default].')
  parser.add_option('--category',
                    help='A list of allowed categories [default: ""].')
  parser.add_option('--throttle', default=100, type='int',
                    help='Delay between events (ms) [default: %default]. ')
  parser.add_option('--seed', type='int',
                    help=('Seed value for pseduo-random generator. Same seed '
                          'value generates the same sequence of events. Seed '
                          'is randomized by default.'))
  parser.add_option('--event-count', default=10000, type='int',
                    help='Number of events to generate [default: %default].')
  parser.add_option('--verbosity', default=1, type='int',
                    help='Verbosity level [0-3] [default: %default].')
  parser.add_option('--extra-args', default='',
                    help=('String of other args to pass to the command verbatim'
                          ' [default: "%default"].'))
  (options, args) = parser.parse_args()

  if args:
    parser.print_help(sys.stderr)
    parser.error('Unknown arguments: %s' % args)

  if not options.package_name:
    parser.print_help(sys.stderr)
    parser.error('Missing package name')

  if options.category:
    options.category = options.category.split(',')

  DispatchPythonTests(options)


if __name__ == '__main__':
  main()
