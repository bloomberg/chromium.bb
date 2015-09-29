#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script runs unit tests of the code in the gpu_tests/ directory.

This script DOES NOT run tests. run_gpu_test does that.
"""

import os
import subprocess
import sys


if __name__ == '__main__':
  gpu_test_dir = os.path.dirname(os.path.realpath(__file__))
  telemetry_dir = os.path.realpath(
    os.path.join(gpu_test_dir, '..', '..', '..', 'tools', 'telemetry'))

  env = os.environ.copy()
  if 'PYTHONPATH' in env:
    env['PYTHONPATH'] = env['PYTHONPATH'] + os.pathsep + telemetry_dir
  else:
    env['PYTHONPATH'] = telemetry_dir

  path_to_run_tests = os.path.join(telemetry_dir, 'telemetry', 'testing',
                                   'run_tests.py')
  argv = ['--top-level-dir', gpu_test_dir] + sys.argv[1:]
  sys.exit(subprocess.call([sys.executable, path_to_run_tests] + argv, env=env))
