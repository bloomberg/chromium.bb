#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import os

import pyauto_functional  # Must be imported before pyauto
import pyauto
import pyauto_utils


class CrashReporterTest(pyauto.PyUITest):
  """TestCase for Crash Reporter."""

  def testRendererCrash(self):
    """Verify renderer's crash reporting.

    Attempts to crash, and then checks that crash dumps get generated.  Does
    not actually test crash reports on the server.
    """
    # bail out if not a branded build
    properties = self.GetBrowserInfo()['properties']
    if properties['branding'] != 'Google Chrome':
      return
    breakpad_folder = properties['DIR_CRASH_DUMPS']
    self.assertTrue(breakpad_folder, 'Cannot figure crash dir')

    unused = pyauto_utils.ExistingPathReplacer(path=breakpad_folder)
    self.NavigateToURL('about:crash')  # Trigger renderer crash
    dmp_files = glob.glob(os.path.join(breakpad_folder, '*.dmp'))
    self.assertEqual(1, len(dmp_files))


if __name__ == '__main__':
  pyauto_functional.Main()

