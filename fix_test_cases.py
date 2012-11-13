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


def with_tempfile(function):
  """Creates a temporary file and calls the inner function."""
  def hook(*args, **kwargs):
    handle, tempfilepath = tempfile.mkstemp(prefix='fix_test_cases')
    os.close(handle)
    try:
      return function(tempfilepath, *args, **kwargs)
    finally:
      try:
        os.remove(tempfilepath)
      except OSError, e:
        print >> sys.stderr, 'Failed to remove %s: %s' % (tempfilepath, e)
  return hook


def load_run_test_cases_results(run_test_cases_file):
  """Loads a .run_test_cases result file.

  Returns a tuple of two lists, (success, failures).
  """
  if not os.path.isfile(run_test_cases_file):
    print >> sys.stderr, 'Failed to find %s' % run_test_cases_file
    return None, None
  with open(run_test_cases_file) as f:
    try:
      data = json.load(f)
    except ValueError as e:
      print >> sys.stderr, ('Unable to load json file, %s: %s' %
                            (run_test_cases_file, str(e)))
      return None, None
  failure = [
    test for test, runs in data['test_cases'].iteritems()
    if not any(not run['returncode'] for run in runs)
  ]
  success = [
    test for test, runs in data['test_cases'].iteritems()
    if any(not run['returncode'] for run in runs)
  ]
  return success, failure


def list_test_cases(isolated):
  """Retrieves the test cases list."""
  cmd = [
    sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
    'run',
    '--isolated', isolated,
    '--',
  ]
  try:
    # Use the default seed.
    # TODO(maruel): Shards?
    return run_test_cases.list_test_cases(
        cmd,
        '.',
        index=0,
        shards=0,
        disabled=False,
        fails=False,
        flaky=False,
        pre=False,
        manual=False,
        seed=1)
  except run_test_cases.Failure, e:
    cmd = [
      sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
      'trace',
      '--merge',
      '--isolated', isolated,
      '--',
      '--gtest_list_tests',
    ]
    add_verbosity(cmd)
    logging.debug(cmd)
    if subprocess.call(cmd):
      # The tracing itself failed.
      raise e
    return run_test_cases.list_test_cases(
        cmd,
        '.',
        index=0,
        shards=0,
        disabled=False,
        fails=False,
        flaky=False,
        pre=False,
        manual=False,
        seed=1)


def add_verbosity(cmd):
  """Adds --verbose flags to |cmd| depending on verbosity."""
  if logging.getLogger().level < logging.ERROR:
    cmd.append('--verbose')
  if logging.getLogger().level < logging.INFO:
    cmd.append('--verbose')


# This function requires 2 temporary files.
@with_tempfile
@with_tempfile
def run_tests(tempfilepath_cases, tempfilepath_result, isolated, test_cases):
  """Runs all the test cases in an isolated environment."""
  with open(tempfilepath_cases, 'w') as f:
    f.write('\n'.join(test_cases))
  cmd = [
    sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
    'run',
    '--isolated', isolated,
    '--',
    '--result', tempfilepath_result,
    '--test-case-file', tempfilepath_cases,
    # Do not retry; it's faster to trace flaky test than retrying each failing
    # tests 3 times.
    # Do not use --run-all, iterate multiple times instead.
    '--retries', '0',
    # Trace at most 25 test cases at a time. While this may seem particularly
    # small, it is because the tracer doesn't scale well on Windows and tends to
    # generate multi-gigabytes data files, that needs to be read N-times the
    # number of test cases. On linux and OSX it's not that much a big deal but
    # if there's 25 test cases that are broken, it's likely that there's a core
    # file missing anyway.
    '--max-failures', '25',
  ]
  add_verbosity(cmd)
  logging.debug(cmd)
  retcode = subprocess.call(cmd)
  success, failures = load_run_test_cases_results(tempfilepath_result)
  # Returning non-zero must match having failures.
  assert bool(retcode) == (failures is None or bool(failures))
  return success, failures


@with_tempfile
def trace_some(tempfilepath, isolated, test_cases):
  """Traces the test cases."""
  with open(tempfilepath, 'w') as f:
    f.write('\n'.join(test_cases))
  cmd = [
    sys.executable, os.path.join(ROOT_DIR, 'isolate_test_cases.py'),
    '--isolated', isolated,
    '--test-case-file', tempfilepath,
    # Do not use --run-all here, we assume the test cases will pass inside the
    # checkout.
  ]
  add_verbosity(cmd)
  logging.debug(cmd)
  return subprocess.call(cmd)


def fix_all(isolated):
  """Runs all the test cases in a gtest executable and trace the failing tests.

  Returns True on success.

  Makes sure the test passes afterward.
  """
  # These could have adverse side-effects.
  # TODO(maruel): Be more intelligent about it, for now be safe.
  blacklist = set(run_test_cases.KNOWN_GTEST_ENV_VARS) - set([
    'GTEST_SHARD_INDEX', 'GTEST_TOTAL_SHARDS'])
  for i in blacklist:
    if i in os.environ:
      print >> sys.stderr, 'Please unset %s' % i
      return False

  # Run until test cases remain to be tested.
  all_test_cases = list_test_cases(isolated)
  remaining_test_cases = all_test_cases[:]
  if not remaining_test_cases:
    print >> sys.stderr, 'Didn\'t find any test case to run'
    return 1

  previous_failures = []
  while remaining_test_cases:
    # pylint is confused about with_tempfile.
    # pylint: disable=E1120
    print(
        '\nTotal: %5d; Remaining: %5d' % (
          len(all_test_cases), len(remaining_test_cases)))
    success, failures = run_tests(isolated, remaining_test_cases)
    if success is None:
      print >> sys.stderr, 'Failed to trace test cases'
      return 1
    print(
        '\nTotal: %5d; Tried to run: %5d; Ran: %5d; Succeeded: %5d; Failed: %5d'
        % (
          len(all_test_cases),
          len(remaining_test_cases),
          len(success) + len(failures),
          len(success),
          len(failures)))

    if not failures:
      print('I\'m done. Have a nice day!')
      return True

    if previous_failures == failures:
      print('The last trace didn\'t help, aborting.')
      return False

    # Test cases that passed to not need to be retried anymore.
    remaining_test_cases = [
      i for i in remaining_test_cases if i not in success and i not in failures
    ]
    # Make sure the failures at put at the end. This way if some tests fails
    # simply because they are broken, and not because of test isolation, the
    # other tests will still be traced.
    remaining_test_cases.extend(failures)

    # Trace the test cases and update the .isolate file.
    print('\nTracing the %d failing tests.' % len(failures))
    if trace_some(isolated, failures):
      # The tracing itself failed.
      return False
    previous_failures = failures


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
