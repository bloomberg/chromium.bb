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

import os
import subprocess
import sys

# Names of test cases to be executed.
TEST_CASES = ['basic', 'capture', 'restore']

# Timeout per test case in seconds.
TIMEOUT_SECS = 30


def main():
  """Runs each test case separately."""

  self_path = os.path.dirname(os.path.abspath(__file__))
  for test_case in TEST_CASES:
    process_run_test = subprocess.Popen([os.path.join(self_path, 'run_test.py'),
                                         test_case, str(TIMEOUT_SECS)],
                                        shell=False)
    process_run_test.wait()
    if process_run_test.returncode != 0:
      print '(TEST CASE FAILURE) %s' % test_case
      sys.exit(process_run_test.returncode)
    else:
      print '(TEST CASE SUCCESS) %s' % test_case

if __name__ == '__main__':
  main()

