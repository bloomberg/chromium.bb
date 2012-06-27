#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import time
import unittest

import pyauto_functional
import pyauto


class MissingRequiredBinaryException(Exception):
  pass


class WebRTCCallTest(pyauto.PyUITest):
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

  def ExtraChromeFlags(self):
    """Adds flags to the Chrome command line."""
    extra_flags = ['--enable-media-stream', '--enable-peer-connection']
    return pyauto.PyUITest.ExtraChromeFlags(self) + extra_flags

  def setUp(self):
    pyauto.PyUITest.setUp(self)

    # Start the peerconnection_server. This must be built before running the
    # test, and we assume the binary ends up next to the Chrome binary.
    binary_path = os.path.join(self.BrowserPath(), 'peerconnection_server')
    if self.IsWin():
      binary_path += '.exe'
    if not os.path.exists(binary_path):
      raise MissingRequiredBinaryException(
        'Could not locate peerconnection_server. Have you built the '
        'peerconnection_server target? We expect to have a '
        'peerconnection_server binary next to the chrome binary.')

    self._server_process = subprocess.Popen(binary_path)

  def tearDown(self):
    self._server_process.kill()

    pyauto.PyUITest.tearDown(self)
    self.assertEquals('', self.CheckErrorsAndCrashes())

  def _SimpleWebRtcCall(self, test_page):
    """Tests we can call and hang up with WebRTC using ROAP/JSEP.

    This test exercises pretty much the whole happy-case for the WebRTC
    JavaScript API. Currently, it exercises a normal call setup using the API
    defined at http://dev.w3.org/2011/webrtc/editor/webrtc.html. The API is
    still evolving.

    The test will load the supplied HTML file, which in turn will load different
    javascript files depending on if we are running ROAP or JSEP.
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

    self.assertEquals('ok-got-stream', self._GetUserMedia(tab_index=0))
    self.assertEquals('ok-got-stream', self._GetUserMedia(tab_index=1))
    self._Connect('user_1', tab_index=0)
    self._Connect('user_2', tab_index=1)

    self._EstablishCall(from_tab_with_index=0)

    self._StartDetectingVideo(tab_index=0, video_element='remote_view')

    self._WaitForVideoToPlay()

    # The hang-up will automatically propagate to the second tab.
    self._HangUp(from_tab_with_index=0)
    self._VerifyHungUp(tab_index=1)

    self._Disconnect(tab_index=0)
    self._Disconnect(tab_index=1)

    # Ensure we didn't miss any errors.
    self._AssertNoFailures(tab_index=0)
    self._AssertNoFailures(tab_index=1)

  def testSimpleWebRtcRoapCall(self):
    self._SimpleWebRtcCall('webrtc_roap_test.html')

  def testSimpleWebRtcJsepCall(self):
    self._SimpleWebRtcCall('webrtc_jsep_test.html')

  def testLocalPreview(self):
    """Brings up a local preview and ensures video is playing.

    This test will launch a window with a single tab and run a getUserMedia call
    which will give us access to the webcam and microphone. Then the javascript
    code will hook up the webcam data to the local_view video tag. We will
    detect video in that tag using the video detector, and if we see video
    moving the test passes.
    """
    url = self.GetFileURLForDataPath('webrtc', 'webrtc_jsep_test.html')
    self.NavigateToURL(url)
    self.assertEquals('ok-got-stream', self._GetUserMedia(tab_index=0))
    self._StartDetectingVideo(tab_index=0, video_element='local_view')

    self._WaitForVideoToPlay()

  def testHandlesNewGetUserMediaRequestSeparately(self):
    """Ensures WebRTC doesn't allow new requests to piggy-back on old ones."""
    url = self.GetFileURLForDataPath('webrtc', 'webrtc_jsep_test.html')
    self.NavigateToURL(url)
    self.AppendTab(pyauto.GURL(url))

    self._GetUserMedia(tab_index=0)
    self._GetUserMedia(tab_index=1)
    self._Connect("user_1", tab_index=0)
    self._Connect("user_2", tab_index=1)

    self._EstablishCall(from_tab_with_index=0)

    self.assertEquals('failed-with-error-1',
                      self._GetUserMedia(tab_index=0, action='deny'))
    self.assertEquals('failed-with-error-1',
                      self._GetUserMedia(tab_index=0, action='dismiss'))

  def _GetUserMedia(self, tab_index, action='allow'):
    """Acquires webcam or mic for one tab and returns the result."""
    self.assertEquals('ok-requested', self.ExecuteJavascript(
        'getUserMedia(true, true)', tab_index=tab_index))

    self.WaitForInfobarCount(1, tab_index=tab_index)
    self.PerformActionOnInfobar(action, infobar_index=0, tab_index=tab_index)
    self._WaitForGetUserMediaResult(tab_index=0)

    result = self._GetUserMediaResult(tab_index=0)
    self._AssertNoFailures(tab_index)
    return result

  def _Connect(self, user_name, tab_index):
    self.assertEquals('ok-connected', self.ExecuteJavascript(
        'connect("http://localhost:8888", "%s")' % user_name,
        tab_index=tab_index))
    self._AssertNoFailures(tab_index)

  def _EstablishCall(self, from_tab_with_index):
    self.assertEquals('ok-call-established', self.ExecuteJavascript(
        'call()', tab_index=from_tab_with_index))
    self._AssertNoFailures(from_tab_with_index)

    # Double-check the call reached the other side.
    self.assertEquals('yes', self.ExecuteJavascript(
        'is_call_active()', tab_index=from_tab_with_index))

  def _HangUp(self, from_tab_with_index):
    self.assertEquals('ok-call-hung-up', self.ExecuteJavascript(
        'hangUp()', tab_index=from_tab_with_index))
    self._VerifyHungUp(from_tab_with_index)
    self._AssertNoFailures(from_tab_with_index)

  def _VerifyHungUp(self, tab_index):
    self.assertEquals('no', self.ExecuteJavascript(
        'is_call_active()', tab_index=tab_index))

  def _Disconnect(self, tab_index):
    self.assertEquals('ok-disconnected', self.ExecuteJavascript(
        'disconnect()', tab_index=tab_index))

  def _WaitForGetUserMediaResult(self, tab_index):
    def HasResult():
      return self._GetUserMediaResult(tab_index) != 'not-called-yet'
    self.assertTrue(self.WaitUntil(HasResult),
                    msg='Timed out while waiting for getUserMedia callback.')

  def _GetUserMediaResult(self, tab_index):
    return self.ExecuteJavascript(
        'obtainGetUserMediaResult()', tab_index=tab_index)

  def _StartDetectingVideo(self, tab_index, video_element):
    self.assertEquals('ok-started', self.ExecuteJavascript(
        'startDetection("%s", "frame_buffer", 320, 240)' % video_element,
        tab_index=tab_index));

  def _WaitForVideoToPlay(self):
    video_playing = self.WaitUntil(
        function=lambda: self.ExecuteJavascript('isVideoPlaying()'),
        expect_retval='video-playing')
    self.assertTrue(video_playing,
                    msg='Timed out while trying to detect video.')

  def _AssertNoFailures(self, tab_index):
    self.assertEquals('ok-no-errors', self.ExecuteJavascript(
        'getAnyTestFailures()', tab_index=tab_index))


if __name__ == '__main__':
  pyauto_functional.Main()
