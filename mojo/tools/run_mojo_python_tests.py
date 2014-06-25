#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import re
import sys
import time
import unittest


def main():
  parser = argparse.ArgumentParser()
  parser.usage = 'run_mojo_python_tests.py [options] [tests...]'
  parser.add_argument('-v', '--verbose', action='count', default=0)
  parser.add_argument('--metadata', action='append', default=[],
                      help=('optional key=value metadata that will be stored '
                            'in the results files (can be used for revision '
                            'numbers, etc.)'))
  parser.add_argument('--write-full-results-to', metavar='FILENAME',
                      action='store',
                      help='path to write the list of full results to.')
  parser.add_argument('tests', nargs='*')

  args = parser.parse_args()

  bad_metadata = False
  for val in args.metadata:
    if '=' not in val:
      print >> sys.stderr, ('Error: malformed metadata "%s"' % val)
      bad_metadata = True
  if bad_metadata:
    print >> sys.stderr
    parser.print_help()
    return 2

  chromium_src_dir = os.path.join(os.path.dirname(__file__),
                                  os.pardir,
                                  os.pardir)

  loader = unittest.loader.TestLoader()
  print "Running Python unit tests under mojo/public/tools/bindings/pylib ..."

  pylib_dir = os.path.join(chromium_src_dir, 'mojo', 'public',
                           'tools', 'bindings', 'pylib')
  if args.tests:
    if pylib_dir not in sys.path:
      sys.path.append(pylib_dir)
    suite = unittest.TestSuite()
    for test_name in args:
      suite.addTests(loader.loadTestsFromName(test_name))
  else:
    suite = loader.discover(pylib_dir, pattern='*_unittest.py')

  runner = unittest.runner.TextTestRunner(verbosity=(args.verbose + 1))
  result = runner.run(suite)

  full_results = _FullResults(suite, result, args.metadata)
  if args.write_full_results_to:
    with open(args.write_full_results_to, 'w') as fp:
      json.dump(full_results, fp, indent=2)
      fp.write("\n")

  return 0 if result.wasSuccessful() else 1


TEST_SEPARATOR = '.'


def _FullResults(suite, result, metadata):
  """Convert the unittest results to the Chromium JSON test result format.

  This matches run-webkit-tests (the layout tests) and the flakiness dashboard.
  """

  full_results = {}
  full_results['interrupted'] = False
  full_results['path_delimiter'] = TEST_SEPARATOR
  full_results['version'] = 3
  full_results['seconds_since_epoch'] = time.time()
  for md in metadata:
    key, val = md.split('=', 1)
    full_results[key] = val

  all_test_names = _AllTestNames(suite)
  failed_test_names = _FailedTestNames(result)

  full_results['num_failures_by_type'] = {
      'Failure': len(failed_test_names),
      'Pass': len(all_test_names) - len(failed_test_names),
  }

  full_results['tests'] = {}

  for test_name in all_test_names:
    value = {
        'expected': 'PASS',
        'actual': 'FAIL' if (test_name in failed_test_names) else 'FAIL',
    }
    _AddPathToTrie(full_results['tests'], test_name, value)

  return full_results


def _AllTestNames(suite):
  test_names = []
  # _tests is protected  pylint: disable=W0212
  for test in suite._tests:
    if isinstance(test, unittest.suite.TestSuite):
      test_names.extend(_AllTestNames(test))
    else:
      test_names.append(_UnitTestName(test))
  return test_names


def _FailedTestNames(result):
  failed_test_names = set()
  for (test, _) in result.failures + result.errors:
    failed_test_names.add(_UnitTestName(test))
  return failed_test_names


def _AddPathToTrie(trie, path, value):
  if TEST_SEPARATOR not in path:
    trie[path] = value
    return
  directory, rest = path.split(TEST_SEPARATOR, 1)
  if directory not in trie:
    trie[directory] = {}
  _AddPathToTrie(trie[directory], rest, value)


_UNITTEST_NAME_REGEX = re.compile("(\w+) \(([\w.]+)\)")


def _UnitTestName(test):
  # This regex and UnitTestName() extracts the test_name in a way
  # that can be handed back to the loader successfully.
  m = _UNITTEST_NAME_REGEX.match(str(test))
  assert m, "could not find test name from test description %s" % str(test)
  return "%s.%s" % (m.group(2), m.group(1))


if __name__ == '__main__':
  sys.exit(main())
