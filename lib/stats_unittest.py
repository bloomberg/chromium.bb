#!/usr/bin/python

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import time
import urllib2

sys.path.insert(0, os.path.abspath('%s/../..' % os.path.dirname(__file__)))
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import stats


class StatsTest(cros_test_lib.MockTestCase):
  """Test the stats creation functionality."""

  def testIt(self):
    """Test normal stats creation, exercising default functionality."""
    cmd_stat = stats.Stats()
    self.assertNotEquals(cmd_stat.host, None)
    self.assertNotEquals(cmd_stat.username, None)


class ConditionsTest(cros_test_lib.MockTestCase):
  # pylint: disable=W0212

  def testConditionsMet(self):
    stat = stats.Stats(
        username='chrome-bot@chromium.org', host='build42-m2.golo.chromium.org')
    self.assertTrue(stats.StatsUploader._UploadConditionsMet(stat))

  def testConditionsMet2(self):
    stat = stats.Stats(
        username='monkey@google.com', host='typewriter.mtv.corp.google.com')
    self.assertTrue(stats.StatsUploader._UploadConditionsMet(stat))

  def testConditionsNotMet(self):
    stat = stats.Stats(
        username='monkey@home.com', host='typewriter.mtv.corp.google.com')
    self.assertFalse(stats.StatsUploader._UploadConditionsMet(stat))

  def testConditionsNotMet2(self):
    stat = stats.Stats(
        username='monkey@google.com', host='typewriter.noname.com')
    self.assertFalse(stats.StatsUploader._UploadConditionsMet(stat))


class UploadTest(cros_test_lib.MockLoggingTestCase):
  """Test the upload functionality.

  For the tests that validate debug log messages are printed, note that unit
  tests are run with debug level logging.DEBUG, so logging.debug() messages in
  the code under test will be displayed.
  """
  # pylint: disable=W0212

  # Increased timeout so that we don't get errors when the machine is loaded.
  TEST_TIMEOUT = 30

  def setUp(self):
    self.cmd_stats = stats.Stats(
        host='test.golo.chromium.org', username='chrome-bot@chromium.org')

  def testNormalRun(self):
    """Going for code coverage."""
    self.PatchObject(urllib2, 'urlopen', autospec=True)
    stats.StatsUploader.Upload(self.cmd_stats)
    with cros_test_lib.LoggingCapturer() as logs:
      # pylint: disable=E1101
      self.assertEquals(urllib2.urlopen.call_count, 1)
      # Make sure no error messages are output in the normal case.
      self.AssertLogsContain(logs, stats.StatsUploader.ENVIRONMENT_ERROR,
                             inverted=True)
      timeout_regex = stats.StatsUploader.TIMEOUT_ERROR % '\d+'
      self.AssertLogsMatch(logs, timeout_regex, inverted=True)

  def CheckSuppressException(self, e, msg):
    """Verifies we don't propagate a given exception during upload."""
    with cros_test_lib.LoggingCapturer() as logs:
      self.PatchObject(stats.StatsUploader, '_Upload', autospec=True,
                       side_effect=e)
      # Verify that the exception is not propagated.
      stats.StatsUploader.Upload(self.cmd_stats, timeout=self.TEST_TIMEOUT)
      # Note: the default log level for unit tests is logging.DEBUG
      self.AssertLogsContain(logs, msg)

  def testUploadTimeoutIgnore(self):
    """We don't propagate timeouts during upload."""
    self.CheckSuppressException(
        cros_build_lib.TimeoutError(),
        stats.StatsUploader.TIMEOUT_ERROR % self.TEST_TIMEOUT)

  def testEnvironmentErrorIgnore(self):
    """We don't propagate any environment errors during upload."""
    msg = stats.StatsUploader.ENVIRONMENT_ERROR
    self.CheckSuppressException(EnvironmentError(), msg)
    self.CheckSuppressException(urllib2.HTTPError(None, None, None, None, None),
                                msg)
    self.CheckSuppressException(urllib2.URLError(""), msg)

  def testKeyboardInterruptError(self):
    """We propagate KeyboardInterrupts."""
    self.PatchObject(stats.StatsUploader, '_Upload', autospec=True,
                     side_effect=KeyboardInterrupt())
    self.assertRaises(KeyboardInterrupt, stats.StatsUploader.Upload,
                      self.cmd_stats, timeout=self.TEST_TIMEOUT)

  def testUploadTimeout(self):
    """We timeout when the upload takes too long."""
    def Sleep(*_args, **_kwargs):
      time.sleep(self.TEST_TIMEOUT)

    self.PatchObject(stats.StatsUploader, '_Upload', autospec=True,
                     side_effect=Sleep)
    with cros_test_lib.LoggingCapturer() as logs:
      stats.StatsUploader.Upload(self.cmd_stats, timeout=1)
      self.AssertLogsContain(logs, stats.StatsUploader.TIMEOUT_ERROR % ('1',))


if __name__ == '__main__':
  cros_test_lib.main()
