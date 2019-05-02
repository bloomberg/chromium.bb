#!/usr/bin/env vpython

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import gpu_project_config

from gpu_tests import path_util

path_util.SetupTelemetryPaths()

from typ import arg_parser

from telemetry.internal.browser import browser_options
from telemetry.testing import serially_executed_browser_test_case
from py_utils import discover

def FindTestCase(test_name):
  for start_dir in gpu_project_config.CONFIG.start_dirs:
    modules_to_classes = discover.DiscoverClasses(
        start_dir,
        gpu_project_config.CONFIG.top_level_dir,
        base_class=serially_executed_browser_test_case.
        SeriallyExecutedBrowserTestCase)
    for cl in modules_to_classes.values():
      if cl.Name() == test_name:
          return cl

def main():
  parser = arg_parser.ArgumentParser()
  options, extra_args = parser.parse_known_args(sys.argv[1:])
  assert options.tests, 'Please pass the test suite\'s name'
  test_case = FindTestCase(options.tests[0])
  assert test_case, 'Test \'%s\' is non-existant' % options.tests[0]
  options = browser_options.BrowserFinderOptions()
  parser = options.CreateParser(test_case.__doc__)
  test_case.AddCommandlineArgs(parser)
  finder_options, _ = parser.parse_args(extra_args)
  # call to set class level arguments
  _ = list(test_case.GenerateGpuTests(finder_options))
  cls = test_case.GetExpectations()
  new_expectations = cls.GenerateNewExpectationsFormat()
  file_name = cls.__module__
  file_name = file_name[file_name.find('.')+1:] + '.txt'
  with open(file_name, 'w') as f:
    f.write(new_expectations)

if __name__ == '__main__':
  sys.exit(main())
