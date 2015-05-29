#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A test runner for gtest application tests."""

import argparse
import json
import logging
import sys
import time

from mopy import gtest
from mopy.android import AndroidShell
from mopy.config import Config


def main():
  parser = argparse.ArgumentParser(description="An application test runner.")
  parser.add_argument("test_list_file", type=file,
                      help="a file listing apptests to run")
  parser.add_argument("build_dir", type=str, help="the build output directory")
  parser.add_argument("--verbose", default=False, action='store_true')
  parser.add_argument('--write-full-results-to', metavar='FILENAME',
                      help='Path to write the JSON list of full results.')
  args = parser.parse_args()

  gtest.set_color()
  logger = logging.getLogger()
  logging.basicConfig(stream=sys.stdout, format="%(levelname)s:%(message)s")
  logger.setLevel(logging.DEBUG if args.verbose else logging.WARNING)
  logger.debug("Initialized logging: level=%s" % logger.level)

  logger.debug("Test list file: %s", args.test_list_file)
  config = Config(args.build_dir)
  execution_globals = {"config": config}
  exec args.test_list_file in execution_globals
  test_list = execution_globals["tests"]
  logger.debug("Test list: %s" % test_list)

  shell = None
  extra_args = []
  if config.target_os == Config.OS_ANDROID:
    shell = AndroidShell(config)
    extra_args.extend(shell.PrepareShellRun('localhost'))

  tests = []
  passed = []
  failed = []
  for test_dict in test_list:
    test = test_dict["test"]
    test_name = test_dict.get("name", test)
    test_type = test_dict.get("type", "gtest")
    test_args = test_dict.get("args", []) + extra_args

    print "Running %s...%s" % (test_name, ("\n" if args.verbose else "")),
    sys.stdout.flush()

    tests.append(test_name)
    assert test_type in ("gtest", "gtest_isolated")
    isolate = test_type == "gtest_isolated"
    result = gtest.run_apptest(config, shell, test_args, test, isolate)
    passed.extend([test_name] if result else [])
    failed.extend([] if result else [test_name])
    print "[  PASSED  ]" if result else "[  FAILED  ]",
    print test_name if args.verbose or not result else ""

  print "[  PASSED  ] %d apptests" % len(passed),
  print ": %s" % ", ".join(passed) if passed else ""
  print "[  FAILED  ] %d apptests" % len(failed),
  print ": %s" % ", ".join(failed) if failed else ""

  if args.write_full_results_to:
    _WriteJSONResults(tests, failed, args.write_full_results_to)

  return 1 if failed else 0


def _WriteJSONResults(tests, failed, write_full_results_to):
  """Write the apptest results in the Chromium JSON test results format.
     See <http://www.chromium.org/developers/the-json-test-results-format>
     TODO(msw): Use Chromium and TYP testing infrastructure.
     TODO(msw): Use GTest Suite.Fixture names, not the apptest names.
     Adapted from chrome/test/mini_installer/test_installer.py
  """
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


if __name__ == '__main__':
  sys.exit(main())
