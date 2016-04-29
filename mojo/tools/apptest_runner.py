#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''A test runner for gtest application tests.'''

import argparse
import json
import logging
import os
import string
import sys
import time

from mopy import gtest
from mopy.config import Config


APPTESTS = os.path.abspath(os.path.join(__file__, '..', 'data', 'apptests'))


def main():
  parser = argparse.ArgumentParser(description='An application test runner.')
  parser.add_argument('build_dir', type=str, help='The build output directory.')
  parser.add_argument('--verbose', default=False, action='store_true',
                      help='Print additional logging information.')
  parser.add_argument('--repeat-count', default=1, metavar='INT',
                      action='store', type=int,
                      help='The number of times to repeat the set of tests.')
  parser.add_argument('--write-full-results-to', metavar='FILENAME',
                      help='The path to write the JSON list of full results.')
  parser.add_argument('--test-launcher-summary-output', metavar='FILENAME',
                      help='The path to write the JSON list of full results.')
  parser.add_argument('--test-list-file', metavar='FILENAME', type=file,
                      default=APPTESTS, help='The file listing tests to run.')
  parser.add_argument('--apptest-filter', default='',
                      help='A comma-separated list of mojo:apptests to run.')
  args, commandline_args = parser.parse_known_args()

  logger = logging.getLogger()
  logging.basicConfig(stream=sys.stdout, format='%(levelname)s:%(message)s')
  logger.setLevel(logging.DEBUG if args.verbose else logging.WARNING)
  logger.debug('Initialized logging: level=%s' % logger.level)

  logger.debug('Test list file: %s', args.test_list_file)
  config = Config(args.build_dir, is_verbose=args.verbose,
                  apk_name='MojoRunnerApptests.apk')
  execution_globals = {'config': config}
  exec args.test_list_file in execution_globals
  test_list = execution_globals['tests']
  logger.debug('Test list: %s' % test_list)

  shell = None
  if config.target_os == Config.OS_ANDROID:
    from mopy.android import AndroidShell
    shell = AndroidShell(config)
    result = shell.InitShell()
    if result != 0:
      return result

  tests = []
  failed = []
  failed_suites = 0
  apptest_filter = [a for a in string.split(args.apptest_filter, ',') if a]
  gtest_filter = [a for a in commandline_args if a.startswith('--gtest_filter')]
  for _ in range(args.repeat_count):
    for test_dict in test_list:
      test = test_dict['test']
      test_name = test_dict.get('name', test)
      test_type = test_dict.get('type', 'gtest')
      test_args = test_dict.get('args', []) + commandline_args
      if apptest_filter and not set(apptest_filter) & set([test, test_name]):
        continue;

      print 'Running %s...%s' % (test_name, ('\n' if args.verbose else '')),
      sys.stdout.flush()

      assert test_type in ('gtest', 'gtest_isolated')
      if test_type == 'gtest':
        print ('WARNING: tests are forced to gtest_isolated until '
               'http://crbug.com/529487 is fixed')
        test_type = 'gtest_isolated'
      isolate = test_type == 'gtest_isolated'
      (ran, fail) = gtest.run_apptest(config, shell, test_args, test, isolate)
      # Ignore empty fixture lists when the commandline has a gtest filter flag.
      if gtest_filter and not ran and not fail:
        print '[ NO TESTS ] ' + (test_name if args.verbose else '')
        continue
      # Use the apptest name if the whole suite failed or no fixtures were run.
      fail = [test_name] if (not ran and (not fail or fail == [test])) else fail
      tests.extend(ran)
      failed.extend(fail)
      result = ran and not fail
      print '[  PASSED  ]' if result else '[  FAILED  ]',
      print test_name if args.verbose or not result else ''
      # Abort when 3 apptest suites, or a tenth of all, have failed.
      # base::TestLauncher does this for timeouts and unknown results.
      failed_suites += 0 if result else 1
      if failed_suites >= max(3, len(test_list) / 10):
        print 'Too many failing suites (%d), exiting now.' % failed_suites
        failed.append('Test runner aborted for excessive failures.')
        break;

    if failed:
      break;

  print '[==========] %d tests ran.' % len(tests)
  print '[  PASSED  ] %d tests.' % (len(tests) - len(failed))
  if failed:
    print '[  FAILED  ] %d tests, listed below:' % len(failed)
    for failure in failed:
      print '[  FAILED  ] %s' % failure

  if args.write_full_results_to:
    _WriteJSONResults(tests, failed, args.write_full_results_to)
  if args.test_launcher_summary_output:
    _WriteSwarmingJSONResults(tests, failed, args.test_launcher_summary_output)

  return 1 if failed else 0


def _WriteJSONResults(tests, failed, write_full_results_to):
  '''Write the apptest results in the Chromium JSON test results format.
     See <http://www.chromium.org/developers/the-json-test-results-format>
     TODO(msw): Use Chromium and TYP testing infrastructure.
     TODO(msw): Use GTest Suite.Fixture names, not the apptest names.
     Adapted from chrome/test/mini_installer/test_installer.py
  '''
  results = {
    'interrupted': False,
    'path_delimiter': '.',
    'version': 3,
    'seconds_since_epoch': time.time(),
    'num_failures_by_type': {
      'FAIL': len(failed),
      'PASS': len(tests) - len(failed),
    },
    'tests': {}
  }

  for test in tests:
    value = {
      'expected': 'PASS',
      'actual': 'FAIL' if test in failed else 'PASS',
      'is_unexpected': True if test in failed else False,
    }
    _AddPathToTrie(results['tests'], test, value)

  with open(write_full_results_to, 'w') as fp:
    json.dump(results, fp, indent=2)
    fp.write('\n')

  return results


def _AddPathToTrie(trie, path, value):
  if '.' not in path:
    trie[path] = value
    return
  directory, rest = path.split('.', 1)
  if directory not in trie:
    trie[directory] = {}
  _AddPathToTrie(trie[directory], rest, value)


def _WriteSwarmingJSONResults(tests, failed, write_full_results_to):
  '''Writes the test results in the JSON test result format used by swarming.'''
  results = {
    'all_tests': tests,
    'disabled_tests': [],
    'global_tags': [],
  }

  test_results = []
  for test in sorted(tests):
    value = [{
      'status': 'FAILURE' if test in failed else 'SUCCESS',
      'output_snippet': '',
      'output_snippet_base64': '',
    }]
    test_results.append({test: value})
  results['per_iteration_data'] = test_results

  with open(write_full_results_to, 'w') as fp:
    json.dump(results, fp, indent=2)
    fp.write('\n')

  return results


if __name__ == '__main__':
  sys.exit(main())
