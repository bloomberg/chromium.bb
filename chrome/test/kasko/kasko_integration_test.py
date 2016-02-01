#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A Windows-only end-to-end integration test for Kasko, Chrome and Crashpad.

This test ensures that the interface between Kasko and Chrome and Crashpad works
as expected. The test causes Kasko to set certain crash keys and invoke a crash
report, which is in turn delivered to a locally hosted test crash server. If the
crash report is received intact with the expected crash keys then all is well.

Note that this test only works against non-component Release and Official builds
of Chrome with Chrome branding, and attempting to use it with anything else will
most likely lead to constant failures.

Typical usage (assuming in root 'src' directory):

- generate project files with the following GYP variables:
    branding=Chrome syzyasan=1 win_z7=0 chromium_win_pch=0
- build the release Chrome binaries:
    ninja -C out\Release chrome.exe chromedriver.exe
- run the test:
    python chrome/test/kasko/kasko_integration_test.py
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

  kasko.integration_test.RunTest(
      options,
      'chrome://kasko/send-report',
      10,
      {'kasko-set-crash-key-value-impl': 'SetCrashKeyValueImpl'})

  _LOGGER.info('Test passed successfully!')


if __name__ == '__main__':
  sys.exit(Main())
