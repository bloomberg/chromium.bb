#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional
import pyauto


class ProcessCountTest(pyauto.PyUITest):
  """Tests to ensure the number of Chrome-related processes is as expected."""

  def _VerifyProcessCount(self, expected_count):
    """Verifies the number of Chrome-related processes is as expected.

    Args:
      expected_count: An integer representing the expected number of Chrome-
                      related processes that should currently exist.
    """
    # Compute the actual number of Chrome-related processes that exist.
    # Processes include: a single browser process; a single renderer process
    # for each tab in each window; 0 or more child processes (associated with
    # plugins or other workers); and 0 or more extension processes.
    info = self.GetBrowserInfo()
    actual_count = (
        1 +  # Browser process.
        sum([len(tab_info['tabs']) for tab_info in info['windows']]) +
        len(info['child_processes']) + len(info['extension_views']))

    self.assertEqual(actual_count, expected_count,
                     msg='Number of processes should be %d, but was %d.' %
                         (expected_count, actual_count))

  def testProcessCountFreshProfile(self):
    """Verifies the process count in a fresh profile."""
    self._VerifyProcessCount(2)

  def testProcessCountAppendSingleTab(self):
    """Verifies the process count after appending a single tab."""
    self.AppendTab(pyauto.GURL('about:blank'), 0)
    self._VerifyProcessCount(3)

  def testProcessCountNewWindow(self):
    """Verifies the process count after opening a new window."""
    self.OpenNewBrowserWindow(True)
    self._VerifyProcessCount(3)

  def testProcessCountFlashProcess(self):
    """Verifies the process count when the flash process is running."""
    flash_url = self.GetFileURLForDataPath('plugin', 'flash.swf')
    self.NavigateToURL(flash_url)
    self._VerifyProcessCount(3)

  def testProcessCountExtensionProcess(self):
    """Verifies the process count when an extension is installed."""
    crx_file_path = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', 'page_action.crx'))
    self.assertTrue(self.InstallExtension(crx_file_path, False),
                    msg='Extension install failed.')
    self._VerifyProcessCount(3)

  def testProcessCountCombination(self):
    """Verifies process count with a combination of tabs/windows/extensions."""
    crx_file_path = os.path.abspath(
        os.path.join(self.DataDir(), 'extensions', 'page_action.crx'))
    self.assertTrue(self.InstallExtension(crx_file_path, False),
                    msg='Extension install failed.')

    for _ in xrange(2):
      self.AppendTab(pyauto.GURL('about:blank'), 0)

    flash_url = self.GetFileURLForDataPath('plugin', 'flash.swf')
    self.NavigateToURL(flash_url)

    self.OpenNewBrowserWindow(True)

    for _ in xrange(3):
      self.AppendTab(pyauto.GURL('about:blank'), 1)

    self._VerifyProcessCount(10)


if __name__ == '__main__':
  pyauto_functional.Main()
