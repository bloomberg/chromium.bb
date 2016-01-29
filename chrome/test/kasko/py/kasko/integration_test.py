#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Integration test for Kasko."""

import logging
import os
import optparse

import kasko


_LOGGER = logging.getLogger(os.path.basename(__file__))


def RunTest(options, url, timeout, expected_keys):
  """Runs an integration test for Kasko crash reports.

  Launches both test server and a Chrome instance and then navigates
  to the test |url|. The visited |url| is expected to generate a
  Kasko report within the |timeout| allowed. The report is finally
  verified against expected crash keys.

  This test raises an exception on error.

  Args:
    options The options used to modify the test behavior. Call
       kasko.config.ParseCommandLine() to generate them.
    url The URL that the browser will be navigated to in order to
      cause a crash.
    timeout The time, in seconds, in which the crash is expected to
      be processed.
    expected_keys A dictionary containing the keys that are expected
      to be present in the crash keys of the report. The value is an
      optional string to give an indication of what is the probable
      cause of the missing key.
  """

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
        chrome.navigate_to(url)

        _LOGGER.info('Waiting for Kasko report')
        if not server.wait_for_report(timeout):
          raise Exception('No Kasko report received.')

    # Verify a few crash keys.
    report = server.crash(0)
    kasko.report.LogCrashKeys(report)
    kasko.report.ValidateCrashReport(report, expected_keys)
