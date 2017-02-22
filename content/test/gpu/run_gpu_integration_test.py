#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import sys

from gpu_tests import path_util
import gpu_project_config

path_util.SetupTelemetryPaths()

from telemetry.testing import browser_test_runner

def PostprocessJSON(file_name, run_test_args):
  def TrimPrefix(s):
    return s[1 + s.rfind('.'):]
  with open(file_name) as f:
    test_result = json.load(f)
  test_result['successes'] = map(TrimPrefix, test_result['successes'])
  test_result['failures'] = map(TrimPrefix, test_result['failures'])
  test_result['run_test_args'] = run_test_args
  with open(file_name, 'w') as f:
    json.dump(test_result, f)

def main():
  rest_args = sys.argv[1:]
  retval = browser_test_runner.Run(
      gpu_project_config.CONFIG, rest_args)
  # Postprocess the outputted JSON to trim all of the prefixes from
  # the test names, to keep them as similar to the old form as
  # possible -- and keep them from getting crazily long.
  parser = argparse.ArgumentParser(description='Temporary argument parser')
  parser.add_argument(
    '--write-abbreviated-json-results-to', metavar='FILENAME',
    action='store',
    help=('Full path for json results'))
  option, _ = parser.parse_known_args(rest_args)
  if option.write_abbreviated_json_results_to:
    PostprocessJSON(option.write_abbreviated_json_results_to, rest_args)
  return retval

if __name__ == '__main__':
  sys.exit(main())
