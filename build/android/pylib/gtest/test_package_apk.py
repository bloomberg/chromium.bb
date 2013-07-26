# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines TestPackageApk to help run APK-based native tests."""

import logging
import os
import shlex
import sys
import tempfile
import time

from pylib import android_commands
from pylib import constants
from pylib import pexpect
from pylib.android_commands import errors

from test_package import TestPackage


class TestPackageApk(TestPackage):
  """A helper class for running APK-based native tests."""

  def __init__(self, adb, device, suite_path_full, tool, test_apk_package_name,
               test_activity_name, command_line_file):
    """
    Args:
      adb: ADB interface the tests are using.
      device: Device to run the tests.
      suite_path_full: Absolute path to a specific test suite to run,
          empty to run all.
          Ex: '/foo/bar/base_unittests-debug.apk', for which
            self.suite_path_full = '/foo/bar/base_unittests-debug.apk'
            self.suite_path = '/foo/bar/base_unittests-debug'
            self.suite_basename = 'base_unittests'
            self.suite_dirname = '/foo/bar'
      tool: Name of the Valgrind tool.
      test_apk_package_name: Apk package name for tests running in APKs.
      test_activity_name: Test activity to invoke for APK tests.
      command_line_file: Filename to use to pass arguments to tests.
    """
    TestPackage.__init__(self, adb, device, suite_path_full, tool)
    self._test_apk_package_name = test_apk_package_name
    self._test_activity_name = test_activity_name
    self._command_line_file = command_line_file

  def _CreateCommandLineFileOnDevice(self, options):
    command_line_file = tempfile.NamedTemporaryFile()
    # GTest expects argv[0] to be the executable path.
    command_line_file.write(self.suite_basename + ' ' + options)
    command_line_file.flush()
    self.adb.PushIfNeeded(command_line_file.name,
                          constants.TEST_EXECUTABLE_DIR + '/' +
                          self._command_line_file)

  def _GetGTestReturnCode(self):
    return None

  def _GetFifo(self):
    # The test.fifo path is determined by:
    # testing/android/java/src/org/chromium/native_test/
    #     ChromeNativeTestActivity.java and
    # testing/android/native_test_launcher.cc
    return '/data/data/' + self._test_apk_package_name + '/files/test.fifo'

  def _ClearFifo(self):
    self.adb.RunShellCommand('rm -f ' + self._GetFifo())

  def _WatchFifo(self, timeout, logfile=None):
    for i in range(10):
      if self.adb.FileExistsOnDevice(self._GetFifo()):
        logging.info('Fifo created.')
        break
      time.sleep(i)
    else:
      raise errors.DeviceUnresponsiveError(
          'Unable to find fifo on device %s ' % self._GetFifo())
    args = shlex.split(self.adb.Adb()._target_arg)
    args += ['shell', 'cat', self._GetFifo()]
    return pexpect.spawn('adb', args, timeout=timeout, logfile=logfile)

  def _StartActivity(self):
    self.adb.StartActivity(
        self._test_apk_package_name,
        self._test_activity_name,
        wait_for_completion=True,
        action='android.intent.action.MAIN',
        force_stop=True)

  def _GetTestSuiteBaseName(self):
    """Returns the  base name of the test suite."""
    # APK test suite names end with '-debug.apk'
    return os.path.basename(self.suite_path).rsplit('-debug', 1)[0]

  #override
  def ClearApplicationState(self):
    self.adb.ClearApplicationState(self._test_apk_package_name)

  #override
  def CreateCommandLineFileOnDevice(self, test_filter, test_arguments):
    self._CreateCommandLineFileOnDevice(
        '--gtest_filter=%s %s' % (test_filter, test_arguments))

  #override
  def GetAllTests(self):
    self._CreateCommandLineFileOnDevice('--gtest_list_tests')
    try:
      self.tool.SetupEnvironment()
      # Clear and start monitoring logcat.
      self._ClearFifo()
      self._StartActivity()
      # Wait for native test to complete.
      p = self._WatchFifo(timeout=30 * self.tool.GetTimeoutScale())
      p.expect('<<ScopedMainEntryLogger')
      p.close()
    finally:
      self.tool.CleanUpEnvironment()
    # We need to strip the trailing newline.
    content = [line.rstrip() for line in p.before.splitlines()]
    return self._ParseGTestListTests(content)

  #override
  def SpawnTestProcess(self):
    try:
      self.tool.SetupEnvironment()
      self._ClearFifo()
      self._StartActivity()
    finally:
      self.tool.CleanUpEnvironment()
    logfile = android_commands.NewLineNormalizer(sys.stdout)
    return self._WatchFifo(timeout=10, logfile=logfile)

  #override
  def Install(self):
    self.tool.CopyFiles()
    self.adb.ManagedInstall(self.suite_path_full, False,
                            package_name=self._test_apk_package_name)

