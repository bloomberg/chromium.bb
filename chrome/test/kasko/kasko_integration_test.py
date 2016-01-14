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
    syzyasan=1 win_z7=0 chromium_win_pch=0
- build the release Chrome binaries:
    ninja -C out\Release chrome.exe
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

  # Generate a temporary directory for use in the tests.
  with kasko.util.ScopedTempDir() as temp_dir:
    # Prevent the temporary directory from self cleaning if requested.
    if options.keep_temp_dirs:
      temp_dir_path = temp_dir.release()
    else:
      temp_dir_path = temp_dir.path

    # Use the specified user data directory if requested.
    if options.user_data_dir:
      user_data_dir = options.user_data_dir
    else:
      user_data_dir = os.path.join(temp_dir_path, 'user-data-dir')

    kasko_dir = os.path.join(temp_dir_path, 'kasko')
    os.makedirs(kasko_dir)

    # Launch the test server.
    server = kasko.crash_server.CrashServer()
    with kasko.util.ScopedStartStop(server):
      _LOGGER.info('Started server on port %d', server.port)

      # Configure the environment so Chrome can find the test crash server.
      os.environ['KASKO_CRASH_SERVER_URL'] = (
          'http://127.0.0.1:%d/crash' % server.port)

      # Launch Chrome and navigate it to the test URL.
      chrome = kasko.process.ChromeInstance(options.chromedriver,
                                            options.chrome, user_data_dir)
      with kasko.util.ScopedStartStop(chrome):
        _LOGGER.info('Navigating to Kasko debug URL')
        chrome.navigate_to('chrome://kasko/send-report')

        _LOGGER.info('Waiting for Kasko report')
        if not server.wait_for_report(10):
          raise Exception('No Kasko report received.')

    report = server.crash(0)
    kasko.report.LogCrashKeys(report)
    kasko.report.ValidateCrashReport(report,
        {'kasko-set-crash-key-value-impl': 'SetCrashKeyValueImpl'})

    return 0


if __name__ == '__main__':
  sys.exit(Main())
