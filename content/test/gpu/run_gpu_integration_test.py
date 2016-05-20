#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import sys

from gpu_tests import path_util
import gpu_project_config

path_util.SetupTelemetryPaths()

from telemetry.testing import browser_test_runner

abbr_json_arg = '--write-abbreviated-json-results-to='

def PostprocessJSON(file_name):
  def TrimPrefix(s):
    return s[1 + s.rfind('.'):]
  with open(file_name) as f:
    test_result = json.load(f)
  test_result['successes'] = map(TrimPrefix, test_result['successes'])
  test_result['failures'] = map(TrimPrefix, test_result['failures'])
  with open(file_name, 'w') as f:
    json.dump(test_result, f)

def main():
  options = browser_test_runner.TestRunOptions()
  rest_args = sys.argv[1:]
  retval = browser_test_runner.Run(
      gpu_project_config.CONFIG, options, rest_args)
  # Postprocess the outputted JSON to trim all of the prefixes from
  # the test names, to keep them as similar to the old form as
  # possible -- and keep them from getting crazily long.
  for arg in rest_args:
    if arg.startswith(abbr_json_arg):
      PostprocessJSON(arg[len(abbr_json_arg):])
      break

if __name__ == '__main__':
  sys.exit(main())
