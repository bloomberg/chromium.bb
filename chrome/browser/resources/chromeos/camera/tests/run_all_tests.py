#!/usr/bin/python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all test cases.

Performs all of the test cases specified in |test_cases|. Returns an error code
on the first failing test case.

To use, run: ./run_all_tests.py. Note, than all of Chrome windows will be
closed.
"""

import argparse
import os
import subprocess
import sys

# Names of test cases to be executed.
TEST_CASES = ['basic', 'capture', 'restore']


def main():
  """Runs each test case separately."""

  parser = argparse.ArgumentParser(description='Runs all of the tests.')
  parser.add_argument('--chrome', help='Path for the Chrome binary.')
  parser.add_argument('--timeout', help='Timeout in seconds.', type=int)
  args, chrome_args = parser.parse_known_args()

  self_path = os.path.dirname(os.path.abspath(__file__))
  for test_case in TEST_CASES:
    run_test_args = [os.path.join(self_path, 'run_test.py')]
    if args.timeout:
      run_test_args.extend(['--timeout', str(args.timeout)])
    if args.chrome:
      run_test_args.extend(['--chrome', args.chrome])
    run_test_args.append(test_case)
    if chrome_args:
      run_test_args.extend(chrome_args)

    process_run_test = subprocess.Popen(run_test_args, shell=False)
    process_run_test.wait()

    if process_run_test.returncode != 0:
      print '(TEST CASE FAILURE) %s' % test_case
      sys.exit(process_run_test.returncode)
    else:
      print '(TEST CASE SUCCESS) %s' % test_case

if __name__ == '__main__':
  main()

