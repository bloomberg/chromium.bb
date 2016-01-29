#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A Windows-only end-to-end integration test for the Chrome hang watcher.

This test ensures that the hang watcher is able to detect when Chrome hangs and
to generate a Kasko report. The report is then delivered to a locally hosted
test crash server. If a crash report is received then all is well.

Note that this test only works against non-component Release and Official builds
of Chrome with Chrome branding, and attempting to use it with anything else will
most likely lead to constant failures.

Typical usage (assuming in root 'src' directory):

- generate project files with the following GYP variables:
    branding=Chrome kasko=1 kasko_hang_reports=1
- build the release Chrome binaries:
    ninja -C out\Release chrome.exe chromedriver.exe
- run the test:
    python chrome/test/kasko/hang_watcher_integration_test.py
"""

import logging
import os
import sys

# Bring in the Kasko module.
KASKO_DIR = os.path.join(os.path.dirname(__file__), 'py')
sys.path.append(KASKO_DIR)
import kasko


_LOGGER = logging.getLogger(os.path.basename(__file__))


def Main():
  options = kasko.config.ParseCommandLine()

  kasko.integration_test.RunTest(options,
                                 'chrome://delayeduithreadhang',
                                 120,
                                 None)

  _LOGGER.info('Test passed successfully!')

  return 0


if __name__ == '__main__':
  sys.exit(Main())
