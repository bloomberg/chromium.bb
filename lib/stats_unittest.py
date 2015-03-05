# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for stats."""

from __future__ import print_function

import time
import urllib2

from chromite.lib import cros_test_lib
from chromite.lib import parallel
from chromite.lib import parallel_unittest
from chromite.lib import partial_mock
from chromite.lib import stats
from chromite.lib import timeout_util


# pylint: disable=W0212


class StatsUploaderMock(partial_mock.PartialMock):
  """Mocks out stats.StatsUploader."""

  TARGET = 'chromite.lib.stats.StatsUploader'
  ATTRS = ('URL', 'UPLOAD_TIMEOUT', '_Upload')

  URL = 'Invalid Url'
  # Increased timeout so that we don't get errors when the machine is loaded.
  UPLOAD_TIMEOUT = 30

  def _Upload(self, _inst, *_args, **_kwargs):
    """Disable actual uploading."""
    return


class StatsMock(partial_mock.PartialMock):
  """Mocks out stats.Stats."""

  TARGET = 'chromite.lib.stats.Stats'
  ATTRS = ('__init__',)

  def __init__(self):
    partial_mock.PartialMock.__init__(self)
    self.init_exception = False

  def _target__init__(self, _inst, **kwargs):
    """Fill in good values for username and host."""
    if self.init_exception:
      raise Exception('abc')

    kwargs.setdefault('username', 'monkey@google.com')
    kwargs.setdefault('host', 'typewriter.mtv.corp.google.com')
    return self.backup['__init__'](_inst, **kwargs)


class StatsModuleMock(partial_mock.PartialMock):
  """Mock out everything needed to use this module."""

  def __init__(self):
    partial_mock.PartialMock.__init__(self)
    self.uploader_mock = StatsUploaderMock()
    self.stats_mock = StatsMock()
    self.parallel_mock = parallel_unittest.ParallelMock()

  def PreStart(self):
    self.StartPatcher(self.uploader_mock)
    self.StartPatcher(self.stats_mock)
    self.StartPatcher(self.parallel_mock)


class StatsCreationTest(cros_test_lib.MockLoggingTestCase):
  """Test the stats creation functionality."""

  def VerifyStats(self, cmd_stat):
    self.assertNotEquals(cmd_stat.host, None)
    self.assertNotEquals(cmd_stat.username, None)

  def testIt(self):
    """Test normal stats creation, exercising default functionality."""
    cmd_stat = stats.Stats()
    self.VerifyStats(cmd_stat)

  def testSafeInitNormal(self):
    """Test normal safe stats creation."""
    cmd_stat = stats.Stats.SafeInit()
    self.VerifyStats(cmd_stat)

  def testSafeInitException(self):
    """Safe stats creation handles exceptions properly."""
    with cros_test_lib.LoggingCapturer() as logs:
      cmd_stat = stats.Stats.SafeInit(monkey='foon')
      self.assertEquals(cmd_stat, None)
      self.AssertLogsContain(logs, 'Exception')


class ConditionsTest(cros_test_lib.MockTestCase):
  """Test UploadConditionsMet."""

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

  def setUp(self):
    self.module_mock = StatsModuleMock()
    self.StartPatcher(self.module_mock)

    self.cmd_stats = stats.Stats(
        host='test.golo.chromium.org', username='chrome-bot@chromium.org')

  def testNormalRun(self):
    """Going for code coverage."""
    self.module_mock.uploader_mock.UnMockAttr('_Upload')
    self.PatchObject(urllib2, 'urlopen', autospec=True)
    stats.StatsUploader.Upload(self.cmd_stats)
    with cros_test_lib.LoggingCapturer() as logs:
      # pylint: disable=E1101
      self.assertEquals(urllib2.urlopen.call_count, 1)
      # Make sure no error messages are output in the normal case.
      self.AssertLogsContain(logs, stats.StatsUploader.ENVIRONMENT_ERROR,
                             inverted=True)
      timeout_regex = stats.StatsUploader.TIMEOUT_ERROR % r'\d+'
      self.AssertLogsMatch(logs, timeout_regex, inverted=True)

  def CheckSuppressException(self, e, msg):
    """Verifies we don't propagate a given exception during upload."""
    with cros_test_lib.LoggingCapturer() as logs:
      stats.StatsUploader._Upload.side_effect = e
      # Verify the exception is suppressed when error_ok=True
      stats.StatsUploader.Upload(self.cmd_stats)
      # Note: the default log level for unit tests is logging.DEBUG
      self.AssertLogsContain(logs, msg)

  def testUploadTimeoutIgnore(self):
    """We don't propagate timeouts during upload."""
    self.CheckSuppressException(
        timeout_util.TimeoutError(),
        stats.StatsUploader.TIMEOUT_ERROR
        % (stats.StatsUploader.UPLOAD_TIMEOUT,))

  def testEnvironmentErrorIgnore(self):
    """We don't propagate any environment errors during upload."""
    url = 'http://somedomainhere.com/foo/bar/uploader'
    env_msg = stats.StatsUploader.ENVIRONMENT_ERROR
    url_msg = stats.StatsUploader.HTTPURL_ERROR % url
    self.CheckSuppressException(EnvironmentError(), env_msg)
    self.CheckSuppressException(urllib2.HTTPError(url, None, None, None, None),
                                url_msg)
    self.CheckSuppressException(urllib2.URLError(""), env_msg)

  def testKeyboardInterruptError(self):
    """We propagate KeyboardInterrupts."""
    stats.StatsUploader._Upload.side_effect = KeyboardInterrupt()
    # Verify the exception is suppressed when error_ok=True
    self.assertRaises(KeyboardInterrupt, stats.StatsUploader.Upload,
                      self.cmd_stats)

  def testUploadTimeout(self):
    """We timeout when the upload takes too long."""
    def Sleep(*_args, **_kwargs):
      time.sleep(stats.StatsUploader.UPLOAD_TIMEOUT)

    stats.StatsUploader._Upload.side_effect = Sleep
    with cros_test_lib.LoggingCapturer() as logs:
      stats.StatsUploader.Upload(self.cmd_stats, timeout=1)
      self.AssertLogsContain(logs, stats.StatsUploader.TIMEOUT_ERROR % ('1',))


class UploadContextTest(cros_test_lib.MockLoggingTestCase):
  """Test the suppression behavior of the upload context."""

  def setUp(self):
    self.StartPatcher(StatsModuleMock())

  def testNoErrors(self):
    """Test that we don't print anything when there are no errors."""
    with cros_test_lib.LoggingCapturer() as logs:
      with stats.UploadContext() as queue:
        queue.put([stats.Stats()])
      self.AssertLogsContain(logs, stats.UNCAUGHT_UPLOAD_ERROR, inverted=True)
      self.assertEquals(stats.StatsUploader._Upload.call_count, 1)

  def testErrorSupression(self):
    """"Test exception supression."""
    for e in [parallel.BackgroundFailure]:
      with cros_test_lib.LoggingCapturer() as logs:
        with stats.UploadContext():
          raise e()
        self.AssertLogsContain(logs, stats.UNCAUGHT_UPLOAD_ERROR)

  def testErrorPropagation(self):
    """Test we propagate some exceptions."""
    def RaiseContext(e):
      with stats.UploadContext():
        raise e()

    for e in [KeyboardInterrupt, RuntimeError, Exception, BaseException,
              SyntaxError,]:
      self.assertRaises(e, RaiseContext, e)


class UploadContextParallelTest(cros_test_lib.MockLoggingTestCase):
  """Test UploadContext using the real parallel library."""

  def testKeyboardInterruptHandling(self):
    """Test that KeyboardInterrupts during upload aren't logged.

    This must use the parallel library so that exceptions are converted into
    BackgroundFailures as they are in a real run.
    """
    self.PatchObject(stats.StatsUploader, '_Upload',
                     side_effect=KeyboardInterrupt())
    with cros_test_lib.LoggingCapturer() as logs:
      with stats.UploadContext() as queue:
        queue.put([stats.Stats()])
      self.AssertLogsContain(logs, stats.UNCAUGHT_UPLOAD_ERROR, inverted=True)


def main(_argv):
  cros_test_lib.main(level='debug', module=__name__)
