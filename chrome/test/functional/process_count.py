#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional
import pyauto


class ProcessCountTest(pyauto.PyUITest):
  """Tests to ensure the number of Chrome-related processes is as expected."""

  FRESH_PROFILE_PROC_COUNT = {
    'win': 2,  # Processes: browser, tab.
    'mac': 2,  # Processes: browser, tab.
    'linux': 4,  # Processes: browser, tab, sandbox helper, zygote.
    'chromeos': 4,  # Processes: browser, tab, sandbox helper, zygote.
  }

  CHROME_PROCESS_NAME = {
    'win': 'chrome.exe',
    'mac': 'Chromium',
    'linux': 'chrome',
    'chromeos': 'chrome',
  }

  def Debug(self):
    """Test method for experimentation.

    This method will not run automatically.
    """
    while True:
      raw_input('Hit <enter> to dump process info...')
      self.pprint(self.GetProcessInfo())

  def setUp(self):
    self.proc_count_fresh_profile = 0
    self.chrome_proc_name = ''
    if self.IsChromeOS():
      self.proc_count_fresh_profile = self.FRESH_PROFILE_PROC_COUNT['chromeos']
      self.chrome_proc_name = self.CHROME_PROCESS_NAME['chromeos']
    elif self.IsWin():
      self.proc_count_fresh_profile = self.FRESH_PROFILE_PROC_COUNT['win']
      self.chrome_proc_name = self.CHROME_PROCESS_NAME['win']
    elif self.IsMac():
      self.proc_count_fresh_profile = self.FRESH_PROFILE_PROC_COUNT['mac']
      self.chrome_proc_name = self.CHROME_PROCESS_NAME['mac']
    elif self.IsLinux():
      self.proc_count_fresh_profile = self.FRESH_PROFILE_PROC_COUNT['linux']
      self.chrome_proc_name = self.CHROME_PROCESS_NAME['linux']

    pyauto.PyUITest.setUp(self)

  def _VerifyProcessCount(self, num_expected):
    """Verifies the number of Chrome-related processes is as expected.

    Args:
      num_expected: An integer representing the expected number of Chrome-
                    related processes that should currently exist.
    """
    proc_info = self.GetProcessInfo()
    browser_info = [x for x in proc_info['browsers']
                    if x['process_name'] == self.chrome_proc_name]
    assert len(browser_info) == 1
    # Utility processes may show up any time.  Ignore them.
    processes = [x for x in browser_info[0]['processes']
                 if x['child_process_type'] != 'Utility']
    num_actual = len(processes)

    self.assertEqual(num_actual, num_expected,
                     msg='Number of processes (ignoring Utility processes) '
                         'should be %d, but was %d.\n'
                         'Actual process info:\n%s' % (
                         num_expected, num_actual, self.pformat(proc_info)))

  def testProcessCountFreshProfile(self):
    """Verifies the process count in a fresh profile."""
    self._VerifyProcessCount(self.proc_count_fresh_profile)

  def testProcessCountAppendSingleTab(self):
    """Verifies the process count after appending a single tab."""
    self.AppendTab(pyauto.GURL('about:blank'), 0)
    self._VerifyProcessCount(self.proc_count_fresh_profile + 1)

  def testProcessCountNewWindow(self):
    """Verifies the process count after opening a new window."""
    self.OpenNewBrowserWindow(True)
    self._VerifyProcessCount(self.proc_count_fresh_profile + 1)

  def testProcessCountFlashProcess(self):
    """Verifies the process count when the flash process is running."""
    flash_url = self.GetFileURLForDataPath('plugin', 'flash.swf')
    self.NavigateToURL(flash_url)
    if self.IsChromeOS():
      # Flash triggers an extra GPU process on ChromeOS.
      self._VerifyProcessCount(self.proc_count_fresh_profile + 2)
    else:
      self._VerifyProcessCount(self.proc_count_fresh_profile + 1)

  def testProcessCountExtensionProcess(self):
    """Verifies the process count when an extension is installed."""
    crx_file_path = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', 'page_action.crx'))
    self.InstallExtension(crx_file_path)
    self._VerifyProcessCount(self.proc_count_fresh_profile + 1)

  def testProcessCountCombination(self):
    """Verifies process count with a combination of tabs/windows/extensions.

    This test installs 1 extension, appends 2 tabs to the window, navigates 1
    tab to a flash page, opens 1 new window, and appends 3 tabs to the new
    window (8 new processes expected).
    """
    if self.IsMac():
      # On Mac 10.5, flash files loaded too quickly after firing browser ends
      # up getting downloaded, which seems to indicate that the plugin hasn't
      # been registered yet.
      # Hack to register Flash plugin on Mac 10.5.  crbug.com/94123
      self.GetPluginsInfo()
    crx_file_path = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', 'page_action.crx'))
    self.InstallExtension(crx_file_path)

    for _ in xrange(2):
      self.AppendTab(pyauto.GURL('about:blank'), 0)

    flash_url = self.GetFileURLForDataPath('plugin', 'flash.swf')
    self.NavigateToURL(flash_url)

    self.OpenNewBrowserWindow(True)

    for _ in xrange(3):
      self.AppendTab(pyauto.GURL('about:blank'), 1)

    if self.IsChromeOS():
      # Flash triggers an extra GPU process on ChromeOS.
      self._VerifyProcessCount(self.proc_count_fresh_profile + 9)
    else:
      self._VerifyProcessCount(self.proc_count_fresh_profile + 8)


if __name__ == '__main__':
  pyauto_functional.Main()
