#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional
import pyauto


class MediaStreamInfobarTest(pyauto.PyUITest):
  """Performs basic tests on the media stream infobar.

  This infobar is used to grant or deny access to WebRTC capabilities for a
  webpage. If a page calls the getUserMedia function the infobar will ask the
  user if it is OK for the webpage to use the webcam or microphone on the user's
  machine. These tests ensure that the infobar works as intended.
  """
  def testAllowingUserMedia(self):
    """Test that selecting 'allow' gives us a media stream.

    When the user clicks allow, the javascript should have the success callback
    called with a media stream.
    """
    self.assertEquals('ok-got-stream',
                      self._TestGetUserMedia(with_action='allow'))

  def testDenyingUserMedia(self):
    """Tests that selecting 'deny' actually denies access to user media.

    When the user clicks deny in the user media bar, the javascript should have
    the error callback called with an error specification instead of the success
    callback with a media stream. This is important since the user should be
    able to deny the javascript to access the webcam.
    """
    # Error 1 = Permission denied
    self.assertEquals('failed-with-error-1',
                      self._TestGetUserMedia(with_action='deny'))

  def testDismissingUserMedia(self):
    """Dismiss should be treated just like deny, which is described above."""
    # Error 1 = Permission denied
    self.assertEquals('failed-with-error-1',
                      self._TestGetUserMedia(with_action='dismiss'))

  def testConsecutiveGetUserMediaCalls(self):
    """Ensures we deal appropriately with several consecutive requests."""
    self.assertEquals('failed-with-error-1',
                      self._TestGetUserMedia(with_action='dismiss'))
    self.assertEquals('failed-with-error-1',
                      self._TestGetUserMedia(with_action='deny'))
    self.assertEquals('ok-got-stream',
                      self._TestGetUserMedia(with_action='allow'))
    self.assertEquals('failed-with-error-1',
                      self._TestGetUserMedia(with_action='deny'))
    self.assertEquals('ok-got-stream',
                      self._TestGetUserMedia(with_action='allow'))
    self.assertEquals('failed-with-error-1',
                      self._TestGetUserMedia(with_action='dismiss'))

  def _TestGetUserMedia(self, with_action):
    """Runs getUserMedia in the test page and returns the result."""
    url = self.GetFileURLForDataPath('webrtc', 'webrtc_test.html')
    self.NavigateToURL(url)

    self.assertEquals('ok-requested', self.ExecuteJavascript(
        'getUserMedia(true, true)'))

    self.WaitForInfobarCount(1)
    self.PerformActionOnInfobar(with_action, infobar_index=0)

    return self.ExecuteJavascript('obtainGetUserMediaResult()')


if __name__ == '__main__':
  pyauto_functional.Main()