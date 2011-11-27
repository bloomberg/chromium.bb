#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import pyauto


class ChromeosBasic(pyauto.PyUITest):
  """Basic tests for ChromeOS.

  Requires ChromeOS to be logged in.
  """

  def testAppendTabs(self):
    """Basic test for primary chrome on ChromeOS (named testing interface)."""
    self.AppendTab(pyauto.GURL('about:version'))
    self.assertEqual(self.GetTabCount(), 2, msg='Expected 2 tabs')

  def testRestart(self):
    """Basic test which involves restarting chrome on ChromeOS."""
    file_url = self.GetFileURLForDataPath('title2.html')
    self.NavigateToURL(file_url)
    self.assertEqual(1, len(self.GetHistoryInfo().History()))
    self.RestartBrowser(clear_profile=False)
    self.assertEqual(1, len(self.GetHistoryInfo().History()))

  def testSetDownloadShelfVisible(self):
    self.assertFalse(self.IsDownloadShelfVisible())
    self.SetDownloadShelfVisible(True)
    self.assertTrue(self.IsDownloadShelfVisible())
    self.SetDownloadShelfVisible(False)
    self.assertFalse(self.IsDownloadShelfVisible())

  def testSetVolume(self):
    """Basic test for setting and getting the volume and mute state."""
    volume_info = self.GetVolumeInfo()
    for mute_setting in (False, True, False):
      self.SetMute(mute_setting)
      self.assertEqual(mute_setting, self.GetVolumeInfo()['is_mute'])
    for volume_setting in (40, 0, 100, 70):
      self.SetVolume(volume_setting)
      self.assertEqual(volume_setting, round(self.GetVolumeInfo()['volume']))

    self.SetVolume(volume_info['volume'])
    self.SetMute(volume_info['is_mute'])
    self.assertEqual(volume_info, self.GetVolumeInfo())


if __name__ == '__main__':
  pyauto_functional.Main()
