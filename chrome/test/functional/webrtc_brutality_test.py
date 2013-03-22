#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import platform

import pyauto_functional
import webrtc_test_base


class WebrtcBrutalityTest(webrtc_test_base.WebrtcTestBase):
  """Tests how WebRTC deals with inconvenient reloads, etc."""

  def testReloadsAfterGetUserMedia(self):
    """Tests how we deal with reloads.

    This test will quickly reload the page after running getUserMedia, which
    will remove the pending request. This crashed the browser before the fix
    for crbug.com/135043.

    The test will make repeated getUserMedia requests with refreshes between
    them. Sometimes it will click past the bar and then refresh.
    """
    self.LoadTestPageInOneTab()
    for i in range(1, 100):
      if i % 10 == 0:
        self.GetUserMedia(tab_index=0, action='accept')
      else:
        self._GetUserMediaWithoutTakingAction(tab_index=0)
      self.ReloadTab(tab_index=0)

  def testRepeatedGetUserMediaRequests(self):
    """Tests how we deal with lots of consecutive getUserMedia requests.

    The test will alternate unanswered requests with requests that get answered.
    """
    if platform.system() == 'Windows' and platform.release() == 'XP':
      print 'Skipping this test on Windows XP due to flakiness.'
      return
    self.LoadTestPageInOneTab()
    for i in range(1, 100):
      if i % 10 == 0:
        self.GetUserMedia(tab_index=0, action='accept')
      else:
        self._GetUserMediaWithoutTakingAction(tab_index=0)

  def testSuccessfulGetUserMediaAndThenReload(self):
    """Waits for WebRTC to respond, and immediately reloads the tab."""
    self.LoadTestPageInOneTab()
    self.GetUserMedia(tab_index=0, action='accept')
    self.ReloadTab(tab_index=0)

  def testClosingTabAfterGetUserMedia(self):
    """Tests closing the tab right after a getUserMedia call."""
    self.LoadTestPageInOneTab()
    self._GetUserMediaWithoutTakingAction(tab_index=0)
    self.CloseTab(tab_index=0)

  def testSuccessfulGetUserMediaAndThenClose(self):
    """Waits for WebRTC to respond, and closes the tab."""
    self.LoadTestPageInOneTab()
    self.GetUserMedia(tab_index=0, action='accept')
    self.CloseTab(tab_index=0)

  def _GetUserMediaWithoutTakingAction(self, tab_index):
    self.assertEquals('ok-requested', self.ExecuteJavascript(
      'getUserMedia("{ audio: true, video: true, }")', tab_index=0))


if __name__ == '__main__':
  pyauto_functional.Main()
