#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import time

import pyauto_functional  # Must be imported before pyauto
import pyauto
import test_utils


class MemoryTest(pyauto.PyUITest):
  """Tests for memory usage of Chrome-related processes.

  These tests are meant to be used manually, not as part of the continuous
  test cycle.  This is because each test starts up and periodically
  measures/records the memory usage of a relevant Chrome process, doing so
  repeatedly until the test is manually killed.  Currently, this script only
  works in Linux and ChromeOS, as it uses a Linux shell command to query the
  system for process memory usage info (test_utils.GetMemoryUsageOfProcess()).

  The tests in this suite produce the following output files (relative to the
  current working directory):

  testTabRendererProcessMemoryUsage: 'renderer_process_mem.txt'
  testExtensionProcessMemoryUsage:   'extension_process_mem.txt'
  """

  # Constants for all tests in this suite.
  NUM_SECONDS_BETWEEN_MEASUREMENTS = 10
  MEASUREMENT_LOG_MESSAGE_TEMPLATE = '[%s] %.2f MB (pid: %d)'
  LOG_TO_OUTPUT_FILE = True

  # Constants for testTabRendererProcessMemoryUsage.
  RENDERER_PROCESS_URL = 'http://chrome.angrybirds.com'
  RENDERER_PROCESS_OUTPUT_FILE = 'renderer_process_mem.txt'

  # Constants for testExtensionProcessMemoryUsage.
  EXTENSION_LOCATION = os.path.abspath(os.path.join(
      pyauto.PyUITest.DataDir(), 'extensions', 'google_talk.crx'))
  EXTENSION_PROCESS_NAME = 'Google Talk'
  EXTENSION_PROCESS_OUTPUT_FILE = 'extension_process_mem.txt'

  def _GetPidOfExtensionProcessByName(self, name):
    """Identifies the process ID of an extension process, given its name.

    Args:
      name: The string name of an extension process, as returned by the function
            GetBrowserInfo().

    Returns:
      The integer process identifier (PID) for the specified process, or
      None if the PID cannot be identified.
    """
    info = self.GetBrowserInfo()['extension_views']
    pid = [x['pid'] for x in info if x['name'] == '%s' % name]
    if pid:
      return pid[0]
    return None

  def _LogMessage(self, log_file, msg):
    """Logs a message to the screen, and to a log file if necessary.

    Args:
      log_file: The string name of a log file to which to write.
      msg: The message to log.
    """
    print msg
    sys.stdout.flush()
    if self.LOG_TO_OUTPUT_FILE:
      print >>open(log_file, 'a'), msg

  def testTabRendererProcessMemoryUsage(self):
    """Test the memory usage of the renderer process for a tab.

    This test periodically queries the system for the current memory usage
    of a tab's renderer process.  The test will take measurements forever; you
    must manually kill the test to terminate it.
    """
    if (self.LOG_TO_OUTPUT_FILE and
        os.path.exists(self.RENDERER_PROCESS_OUTPUT_FILE)):
      os.remove(self.RENDERER_PROCESS_OUTPUT_FILE)
    self.NavigateToURL(self.RENDERER_PROCESS_URL)
    self._LogMessage(
        self.RENDERER_PROCESS_OUTPUT_FILE,
        'Memory usage for renderer process of a tab navigated to: "%s"' % (
            self.RENDERER_PROCESS_URL))

    # A user must manually kill this test to terminate the following loop.
    while True:
      pid = self.GetBrowserInfo()['windows'][0]['tabs'][0]['renderer_pid']
      usage = test_utils.GetMemoryUsageOfProcess(pid)
      current_time = time.asctime(time.localtime(time.time()))
      self._LogMessage(
          self.RENDERER_PROCESS_OUTPUT_FILE,
          self.MEASUREMENT_LOG_MESSAGE_TEMPLATE % (current_time, usage, pid))
      time.sleep(self.NUM_SECONDS_BETWEEN_MEASUREMENTS)

  def testExtensionProcessMemoryUsage(self):
    """Test the memory usage of an extension process.

    This test periodically queries the system for the current memory usage
    of an extension process.  The test will take measurements forever; you
    must manually kill the test to terminate it.
    """
    if (self.LOG_TO_OUTPUT_FILE and
        os.path.exists(self.EXTENSION_PROCESS_OUTPUT_FILE)):
      os.remove(self.EXTENSION_PROCESS_OUTPUT_FILE)
    self.InstallExtension(self.EXTENSION_LOCATION)
    # The PID is 0 until the extension has a chance to start up.
    self.WaitUntil(
        lambda: self._GetPidOfExtensionProcessByName(
                    self.EXTENSION_PROCESS_NAME) not in [0, None])
    self._LogMessage(
        self.EXTENSION_PROCESS_OUTPUT_FILE,
        'Memory usage for extension process with name: "%s"' % (
            self.EXTENSION_PROCESS_NAME))

    # A user must manually kill this test to terminate the following loop.
    while True:
      pid = self._GetPidOfExtensionProcessByName(self.EXTENSION_PROCESS_NAME)
      usage = test_utils.GetMemoryUsageOfProcess(pid)
      current_time = time.asctime(time.localtime(time.time()))
      self._LogMessage(
          self.EXTENSION_PROCESS_OUTPUT_FILE,
          self.MEASUREMENT_LOG_MESSAGE_TEMPLATE % (current_time, usage, pid))
      time.sleep(self.NUM_SECONDS_BETWEEN_MEASUREMENTS)


if __name__ == '__main__':
  pyauto_functional.Main()
