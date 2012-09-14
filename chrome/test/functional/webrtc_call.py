#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess

import pyauto_functional
import pyauto
import webrtc_test_base


class WebrtcCallTest(webrtc_test_base.WebrtcTestBase):
  """Test we can set up a WebRTC call and disconnect it.

  Prerequisites: This test case must run on a machine with a webcam, either
  fake or real, and with some kind of audio device. You must make the
  peerconnection_server target before you run.

  The test case will launch a custom binary
  (peerconnection_server) which will allow two WebRTC clients to find each
  other. For more details, see the source code which is available at the site
  http://code.google.com/p/libjingle/source/browse/ (make sure to browse to
  trunk/talk/examples/peerconnection/server).
  """

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self.StartPeerConnectionServer()

  def tearDown(self):
    self.StopPeerConnectionServer()

    pyauto.PyUITest.tearDown(self)
    self.assertEquals('', self.CheckErrorsAndCrashes())

  def _SimpleWebrtcCall(self, test_page):
    """Tests we can call and hang up with WebRTC.

    This test exercises pretty much the whole happy-case for the WebRTC
    JavaScript API. Currently, it exercises a normal call setup using the API
    defined at http://dev.w3.org/2011/webrtc/editor/webrtc.html. The API is
    still evolving.

    The test will load the supplied HTML file, which in turn will load different
    javascript files depending on which version of the signaling protocol
    we are running.
    The supplied HTML file will be loaded in two tabs and tell the web
    pages to start up WebRTC, which will acquire video and audio devices on the
    system. This will launch a dialog in Chrome which we click past using the
    automation controller. Then, we will order both tabs to connect the server,
    which will make the two tabs aware of each other. Once that is done we order
    one tab to call the other.

    We make sure that the javascript tells us that the call succeeded, lets it
    run for a while and try to hang up the call after that. We verify video is
    playing by using the video detector.
    """
    url = self.GetFileURLForDataPath('webrtc', test_page)
    self.NavigateToURL(url)
    self.AppendTab(pyauto.GURL(url))

    self.assertEquals('ok-got-stream', self.GetUserMedia(tab_index=0))
    self.assertEquals('ok-got-stream', self.GetUserMedia(tab_index=1))
    self.Connect('user_1', tab_index=0)
    self.Connect('user_2', tab_index=1)

    self.EstablishCall(from_tab_with_index=0)

    self._StartDetectingVideo(tab_index=0, video_element='remote-view')

    self._WaitForVideoToPlay()

    # The hang-up will automatically propagate to the second tab.
    self.HangUp(from_tab_with_index=0)
    self.VerifyHungUp(tab_index=1)

    self.Disconnect(tab_index=0)
    self.Disconnect(tab_index=1)

    # Ensure we didn't miss any errors.
    self.AssertNoFailures(tab_index=0)
    self.AssertNoFailures(tab_index=1)

  def testSimpleWebrtcJsepCall(self):
    """Uses a draft of the PeerConnection API, using JSEP00."""
    self._SimpleWebrtcCall('webrtc_jsep_test.html')

  def testSimpleWebrtcJsep01Call(self):
    """Uses a draft of the PeerConnection API, using JSEP01."""
    self._SimpleWebrtcCall('webrtc_jsep01_test.html')

  def testLocalPreview(self):
    """Brings up a local preview and ensures video is playing.

    This test will launch a window with a single tab and run a getUserMedia call
    which will give us access to the webcam and microphone. Then the javascript
    code will hook up the webcam data to the local-view video tag. We will
    detect video in that tag using the video detector, and if we see video
    moving the test passes.
    """
    url = self.GetFileURLForDataPath('webrtc', 'webrtc_jsep_test.html')
    self.NavigateToURL(url)
    self.assertEquals('ok-got-stream', self.GetUserMedia(tab_index=0))
    self._StartDetectingVideo(tab_index=0, video_element='local-view')

    self._WaitForVideoToPlay()

  def testHandlesNewGetUserMediaRequestSeparately(self):
    """Ensures WebRTC doesn't allow new requests to piggy-back on old ones."""
    url = self.GetFileURLForDataPath('webrtc', 'webrtc_jsep_test.html')
    self.NavigateToURL(url)
    self.AppendTab(pyauto.GURL(url))

    self.GetUserMedia(tab_index=0)
    self.GetUserMedia(tab_index=1)
    self.Connect("user_1", tab_index=0)
    self.Connect("user_2", tab_index=1)

    self.EstablishCall(from_tab_with_index=0)

    self.assertEquals('failed-with-error-1',
                      self.GetUserMedia(tab_index=0, action='deny'))
    self.assertEquals('failed-with-error-1',
                      self.GetUserMedia(tab_index=0, action='dismiss'))

  def _StartDetectingVideo(self, tab_index, video_element):
    self.assertEquals('ok-started', self.ExecuteJavascript(
        'startDetection("%s", "frame-buffer", 320, 240)' % video_element,
        tab_index=tab_index));

  def _WaitForVideoToPlay(self):
    video_playing = self.WaitUntil(
        function=lambda: self.ExecuteJavascript('isVideoPlaying()'),
        expect_retval='video-playing')
    self.assertTrue(video_playing,
                    msg='Timed out while trying to detect video.')


if __name__ == '__main__':
  pyauto_functional.Main()
