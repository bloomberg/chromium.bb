# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base class for linker-specific test cases.

   The custom dynamic linker can only be tested through a custom test case
   for various technical reasons:

     - It's an 'invisible feature', i.e. it doesn't expose a new API or
       behaviour, all it does is save RAM when loading native libraries.

     - Checking that it works correctly requires several things that do not
       fit the existing GTest-based and instrumentation-based tests:

         - Native test code needs to be run in both the browser and renderer
           process at the same time just after loading native libraries, in
           a completely asynchronous way.

         - Each test case requires restarting a whole new application process
           with a different command-line.

         - Enabling test support in the Linker code requires building a special
           APK with a flag to activate special test-only support code in the
           Linker code itself.

       Host-driven tests have also been tried, but since they're really
       sub-classes of instrumentation tests, they didn't work well either.

   To build and run the linker tests, do the following:

     ninja -C out/Debug content_linker_test_apk
     build/android/test_runner.py linker

   The core of the checks performed here are pretty simple:

     - Clear the logcat and start recording with an appropriate set of filters.
     - Create the command-line appropriate for the test-case.
     - Start the activity (always forcing a cold start).
     - Every second, look at the current content of the filtered logcat lines
       and look for instances of the following:

          BROWSER_LINKER_TEST: <status>
          RENDERER_LINKER_TEST: <status>

       where <status> can be either FAIL or SUCCESS. These lines can appear
       in any order in the logcat. Once both browser and renderer status are
       found, stop the loop. Otherwise timeout after 30 seconds.

       Note that there can be other lines beginning with BROWSER_LINKER_TEST:
       and RENDERER_LINKER_TEST:, but are not followed by a <status> code.

     - The test case passes if the <status> for both the browser and renderer
       process are SUCCESS. Otherwise its a fail.
"""

import logging
import os
import re
import StringIO
import subprocess
import tempfile
import time

from pylib import android_commands
from pylib import flag_changer
from pylib.base import base_test_result


_PACKAGE_NAME='org.chromium.content_linker_test_apk'
_ACTIVITY_NAME='.ContentLinkerTestActivity'
_COMMAND_LINE_FILE='/data/local/tmp/content-linker-test-command-line'

# Logcat filters used during each test. Only the 'chromium' one is really
# needed, but the logs are added to the TestResult in case of error, and
# it is handy to have the 'content_android_linker' ones as well when
# troubleshooting.
_LOGCAT_FILTERS = [ '*:s', 'chromium:v', 'content_android_linker:v' ]

# Regular expression used to match status lines in logcat.
re_status_line = re.compile(r'(BROWSER|RENDERER)_LINKER_TEST: (FAIL|SUCCESS)')

def _CheckLinkerTestStatus(logcat):
  """Parse the content of |logcat| and checks for both a browser and
     renderer status line.

  Args:
    logcat: A string to parse. Can include line separators.

  Returns:
    A tuple, result[0] is True if there is a complete match, then
    result[1] and result[2] will be True or False to reflect the
    test status for the browser and renderer processes, respectively.
  """
  browser_found = False
  renderer_found = False
  for m in re_status_line.finditer(logcat):
    process_type, status = m.groups()
    if process_type == 'BROWSER':
      browser_found = True
      browser_success = (status == 'SUCCESS')
    elif process_type == 'RENDERER':
      renderer_found = True
      renderer_success = (status == 'SUCCESS')
    else:
      assert False, 'Invalid process type ' + process_type

  if browser_found and renderer_found:
    return (True, browser_success, renderer_success)

  # Didn't find anything.
  return (False, None, None)


def _CreateCommandLineFileOnDevice(adb, flags):
  changer = flag_changer.FlagChanger(adb, _COMMAND_LINE_FILE)
  changer.Set(flags)


class LinkerTestCase(object):
  """Base class for linker test cases."""

  def __init__(self, test_name, is_low_memory=False):
    """Create a test case initialized to run |test_name|.

    Args:
      test_name: The name of the method to run as the test.
      is_low_memory: True to simulate a low-memory device, False otherwise.
    """
    self.test_name = test_name
    class_name = self.__class__.__name__
    self.qualified_name = '%s.%s' % (class_name, self.test_name)
    self.tagged_name = self.qualified_name
    self.is_low_memory = is_low_memory

  def Run(self, device):
    margin = 8
    print '[ %-*s ] %s' % (margin, 'RUN', self.tagged_name)
    logging.info('Running linker test: %s', self.tagged_name)
    adb = android_commands.AndroidCommands(device)

    # 1. Write command-line file with appropriate options.
    command_line_flags = []
    if self.is_low_memory:
      command_line_flags.append('--low-memory-device')
    _CreateCommandLineFileOnDevice(adb, command_line_flags)

    # 2. Start recording logcat with appropriate filters.
    adb.StartRecordingLogcat(clear=True, filters=_LOGCAT_FILTERS)

    try:
      # 3. Force-start activity.
      adb.StartActivity(package=_PACKAGE_NAME,
                        activity=_ACTIVITY_NAME,
                        force_stop=True)

      # 4. Wait up to 30 seconds until the linker test status is in the logcat.
      max_tries = 30
      num_tries = 0
      found = False
      logcat = None
      while num_tries < max_tries:
        time.sleep(1)
        num_tries += 1
        found, browser_ok, renderer_ok = _CheckLinkerTestStatus(
            adb.GetCurrentRecordedLogcat())
        if found:
          break

    finally:
      # Ensure the ADB polling process is always killed when
      # the script is interrupted by the user with Ctrl-C.
      logs = adb.StopRecordingLogcat()

    results = base_test_result.TestRunResults()

    if num_tries >= max_tries:
      # Timeout
      print '[ %*s ] %s' % (margin, 'TIMEOUT', self.tagged_name)
      results.AddResult(
          base_test_result.BaseTestResult(
              self.test_name,
              base_test_result.ResultType.TIMEOUT,
              logs))
    elif browser_ok and renderer_ok:
      # Passed
      logging.info(
          'Logcat start ---------------------------------\n%s' +
          'Logcat end -----------------------------------', logs)
      print '[ %*s ] %s' % (margin, 'OK', self.tagged_name)
      results.AddResult(
          base_test_result.BaseTestResult(
              self.test_name,
              base_test_result.ResultType.PASS))
    else:
      print '[ %*s ] %s' % (margin, 'FAILED', self.tagged_name)
      # Failed
      results.AddResult(
          base_test_result.BaseTestResult(
              self.test_name,
              base_test_result.ResultType.FAIL,
              logs))

    return results

  def __str__(self):
    return self.tagged_name

  def __repr__(self):
    return self.tagged_name
