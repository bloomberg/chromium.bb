#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import random

# Note: pyauto_functional must come before pyauto.
import pyauto_functional
import pyauto
import webrtc_test_base


class WebrtcApprtcCallTest(webrtc_test_base.WebrtcTestBase):
  """Tests calling apprtc.appspot.com and setting up a call.

  Prerequisites: This test case must run on a machine with a webcam, either
  fake or real, and with some kind of audio device. The machine must have access
  to the public Internet.

  This should be considered an integration test: test failures could mean
  that the AppRTC reference is broken, that WebRTC is broken, or both.
  """

  def tearDown(self):
    pyauto.PyUITest.tearDown(self)
    self.assertEquals('', self.CheckErrorsAndCrashes(),
                      'Chrome crashed or hit a critical error during test.')

  def testApprtcLoopbackCall(self):
    self.NavigateToURL('http://apprtc.appspot.com/?debug=loopback')
    self.WaitForInfobarCount(1, tab_index=0)
    self.PerformActionOnInfobar('accept', infobar_index=0, tab_index=0)

    self._WaitForCallEstablishment(tab_index=0)

  def testApprtcTabToTabCall(self):
    # Randomize the call session id. If we would use the same id we would risk
    # getting problems with hung calls and lingering state in AppRTC.
    random_call_id = 'pyauto%d' % random.randint(0, 65536)
    apprtc_url = 'http://apprtc.appspot.com/?r=%s' % random_call_id

    self.NavigateToURL(apprtc_url)
    self.AppendTab(pyauto.GURL(apprtc_url))

    self.WaitForInfobarCount(1, tab_index=0)
    self.WaitForInfobarCount(1, tab_index=1)

    self.PerformActionOnInfobar('accept', infobar_index=0, tab_index=0)
    self.PerformActionOnInfobar('accept', infobar_index=0, tab_index=1)

    self._WaitForCallEstablishment(tab_index=0)
    self._WaitForCallEstablishment(tab_index=1)

  def _WaitForCallEstablishment(self, tab_index):
    # AppRTC will set opacity to 1 for remote video when the call is up.
    video_playing = self.WaitUntil(
        function=lambda: self.GetDOMValue('remoteVideo.style.opacity',
                                          tab_index=tab_index),
        expect_retval='1')
    self.assertTrue(video_playing,
                    msg=('Timed out while waiting for '
                         'remoteVideo.style.opacity to return 1.'))


if __name__ == '__main__':
  pyauto_functional.Main()
