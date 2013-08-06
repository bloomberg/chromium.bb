#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the Monkey tests on one or more devices."""
import logging
import optparse
import random
import sys

from pylib.base import base_test_result
from pylib.base import test_dispatcher
from pylib.host_driven import test_case
from pylib.host_driven import test_runner
from pylib.utils import report_results
from pylib.utils import test_options_parser


class MonkeyTest(test_case.HostDrivenTestCase):
  def __init__(self, test_name, package_name, activity_name, category, seed,
               throttle, event_count, verbosity, extra_args):
    """Create a MonkeyTest object.

    Args:
      test_name: Name of the method to run for this test object.
      package_name: Allowed package.
      activity_name: Name of the activity to start.
      category: A list of allowed categories.
      seed: Seed value for pseduo-random generator. Same seed value
          generates the same sequence of events. Seed is randomized by default.
      throttle: Delay between events (ms).
      event_count: Number of events to generate.
      verbosity: Verbosity level [0-3].
      extra_args: A string of other args to pass to the command verbatim.
    """
    super(MonkeyTest, self).__init__(test_name)
    self.package_name = package_name
    self.activity_name = activity_name
    self.category = category
    self.seed = seed or random.randint(1, 100)
    self.throttle = throttle
    self.event_count = event_count
    self.verbosity = verbosity
    self.extra_args = extra_args

  def testMonkey(self):
    # Launch and wait for Chrome to launch.
    self.adb.StartActivity(self.package_name,
                           self.activity_name,
                           wait_for_completion=True,
                           action='android.intent.action.MAIN',
                           force_stop=True)

    # Chrome crashes are not always caught by Monkey test runner.
    # Verify Chrome has the same PID before and after the test.
    before_pids = self.adb.ExtractPid(self.package_name)

    # Run the test.
    output = ''
    if before_pids:
      output = '\n'.join(self._LaunchMonkeyTest())
      after_pids = self.adb.ExtractPid(self.package_name)

    crashed = (not before_pids or not after_pids
               or after_pids[0] != before_pids[0])

    results = base_test_result.TestRunResults()
    if 'Monkey finished' in output and not crashed:
      result = base_test_result.BaseTestResult(
          self.tagged_name, base_test_result.ResultType.PASS, log=output)
    else:
      result = base_test_result.BaseTestResult(
          self.tagged_name, base_test_result.ResultType.FAIL, log=output)
    results.AddResult(result)
    return results

  def _LaunchMonkeyTest(self):
    """Runs monkey test for a given package.

    Returns:
      Output from the monkey command on the device.
    """

    timeout_ms = self.event_count * self.throttle * 1.5

    cmd = ['monkey',
           '-p %s' % self.package_name,
           ' '.join(['-c %s' % c for c in self.category]),
           '--throttle %d' % self.throttle,
           '-s %d' % self.seed,
           '-v ' * self.verbosity,
           '--monitor-native-crashes',
           '--kill-process-after-error',
           self.extra_args,
           '%d' % self.event_count]
    return self.adb.RunShellCommand(' '.join(cmd), timeout_time=timeout_ms)


def RunMonkeyTests(options):
  """Runs the Monkey tests, replicating it if there multiple devices."""
  logger = logging.getLogger()
  logger.setLevel(logging.DEBUG)

  # Actually run the tests.
  logging.debug('Running monkey tests.')
  available_tests = [
      MonkeyTest('testMonkey', options.package_name, options.activity_name,
                 category=options.category, seed=options.seed,
                 throttle=options.throttle, event_count=options.event_count,
                 verbosity=options.verbosity, extra_args=options.extra_args)]

  def TestRunnerFactory(device, shard_index):
    return test_runner.HostDrivenTestRunner(
        device, shard_index, '', options.build_type, False, False)

  results, exit_code = test_dispatcher.RunTests(
      available_tests, TestRunnerFactory, False, None, shard=False,
      build_type=options.build_type, num_retries=0)

  report_results.LogFull(
      results=results,
      test_type='Monkey',
      test_package='Monkey',
      build_type=options.build_type)

  return exit_code


def main():
  desc = 'Run the Monkey tests on 1 or more devices.'
  parser = optparse.OptionParser(description=desc)
  test_options_parser.AddBuildTypeOption(parser)
  parser.add_option('--package-name', help='Allowed package.')
  parser.add_option('--activity-name',
                    default='com.google.android.apps.chrome.Main',
                    help='Name of the activity to start [default: %default].')
  parser.add_option('--category', default='',
                    help='A list of allowed categories [default: %default].')
  parser.add_option('--throttle', default=100, type='int',
                    help='Delay between events (ms) [default: %default]. ')
  parser.add_option('--seed', type='int',
                    help=('Seed value for pseudo-random generator. Same seed '
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

  RunMonkeyTests(options)


if __name__ == '__main__':
  main()
