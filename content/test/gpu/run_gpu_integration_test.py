#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from gpu_tests import path_util
import gpu_project_config

path_util.SetupTelemetryPaths()

from telemetry.testing import browser_test_runner


def main():
  options = browser_test_runner.TestRunOptions()
  return browser_test_runner.Run(
      gpu_project_config.CONFIG, options, sys.argv[1:])


if __name__ == '__main__':
  sys.exit(main())
