#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs through isolate_test_cases.py all the tests cases in a google-test
executable, grabs the failures and traces them to generate a new .isolate.

This scripts requires a .isolated file. This file is generated from a .isolate
file. You can use 'GYP_DEFINES=test_isolation_mode=check ninja foo_test_run' to
generate it.
"""

import json
import logging
import optparse
import os
import subprocess
import sys
import tempfile

import run_test_cases

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))


def load_run_test_cases_results(run_test_cases_file):
  """Loads a .run_test_cases result file.

  Returns a tuple of two lists, (success, failures).
  """
  if not os.path.isfile(run_test_cases_file):
    print >> sys.stderr, 'Failed to find %s' % run_test_cases_file
    return None
  with open(run_test_cases_file) as f:
    try:
      data = json.load(f)
    except ValueError as e:
      print >> sys.stderr, ('Unable to load json file, %s: %s' %
                            (run_test_cases_file, str(e)))
      return None
  failure = [
    test for test, runs in data['test_cases'].iteritems()
    if not any(not run['returncode'] for run in runs)
  ]
  success = [
    test for test, runs in data['test_cases'].iteritems()
    if any(not run['returncode'] for run in runs)
  ]
  return success, failure


def run_all(isolated, run_test_cases_file):
  """Runs the test cases in an isolated environment."""
  cmd = [
    sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
    'run',
    '--isolated', isolated,
    '--',
    '--result', run_test_cases_file,
    '--run-all',
  ]
  logging.debug(cmd)
  return subprocess.call(cmd)


def trace_all(isolated, test_cases):
  """Traces the test cases."""
  handle, test_cases_file = tempfile.mkstemp(prefix='fix_test_cases')
  os.write(handle, '\n'.join(test_cases))
  os.close(handle)
  try:
    cmd = [
      sys.executable, os.path.join(ROOT_DIR, 'isolate_test_cases.py'),
      '-r', isolated,
      '--test-case-file', test_cases_file,
      '-v',
    ]
    logging.debug(cmd)
    return subprocess.call(cmd)
  finally:
    os.remove(test_cases_file)


def fix_all(isolated):
  """Runs all the test cases in a gtest executable and trace the failing tests.

  Returns True on success.

  Makes sure the test passes afterward.
  """
  # These could have adverse side-effects.
  # TODO(maruel): Be more intelligent about it, for now be safe.
  for i in run_test_cases.KNOWN_GTEST_ENV_VARS:
    if i in os.environ:
      print >> 'Please unset %s' % i
      return False

  handle, run_test_cases_file = tempfile.mkstemp(prefix='fix_test_cases')
  os.close(handle)
  try:
    run_all(isolated, run_test_cases_file)
    success, failures = load_run_test_cases_results(run_test_cases_file)
    print(
        '\nFound %d working and %d broken test cases.' %
        (len(success), len(failures)))
    if not failures:
      return True

    # Trace them all and update the .isolate file.
    print('\nTracing the failing tests.')
    if trace_all(isolated, failures):
      return False

    print('\nRunning again to confirm.')
    run_all(isolated, run_test_cases_file)
    fixed_success, fixed_failures = load_run_test_cases_results(
        run_test_cases_file)

    print(
        '\nFound %d working and %d broken test cases.' %
        (len(fixed_success), len(fixed_failures)))
    for failure in fixed_failures:
      print('  %s' % failure)
    return not fixed_failures
  finally:
    os.remove(run_test_cases_file)


def main():
  parser = optparse.OptionParser(
      usage='%prog <option>',
      description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-s', '--isolated',
      help='The isolated file')
  parser.add_option(
      '-v', '--verbose', action='store_true',
      help='Logs information')
  options, args = parser.parse_args()

  logging.basicConfig(
      level=(logging.DEBUG if options.verbose else logging.ERROR))
  if args:
    parser.error('Unsupported arg: %s' % args)
  if not options.isolated:
    parser.error('--isolated is required')
  if not options.isolated.endswith('.isolated'):
    parser.error('--isolated argument must end with .isolated')
  return not fix_all(options.isolated)


if __name__ == '__main__':
  sys.exit(main())
