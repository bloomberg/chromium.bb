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


class WebRTCCallTest(pyauto.PyUITest):
  """Test we can set up a WebRTC call and disconnect it.

  This test case must run on a machine with a webcam, either fake or real, and
  with some kind of audio device. The test case will launch a custom binary
  (peerconnection_server) which will allow two WebRTC clients to find each
  other. For more details, see the source code which is available at the site
  http://code.google.com/p/libjingle/source/browse/ (make sure to browse to
  trunk/talk/examples/peerconnection/server).
  """

  def setUp(self):
    pyauto.PyUITest.setUp(self)

    # Start the peerconnection_server. Note: this only works on Linux for now.
    binary_path = os.path.join(self.DataDir(), 'webrtc', 'linux',
                               'peerconnection_server')
    self._server_process = subprocess.Popen(binary_path)

  def tearDown(self):
    self._server_process.kill()

    pyauto.PyUITest.tearDown(self)

  def testCanBringUpAndTearDownWebRtcCall(self):
    """Tests we can call and hang up with WebRTC.

    This test exercises pretty much the whole happy-case for the WebRTC
    JavaScript API. Currently, it exercises a normal call setup using the API
    defined at http://dev.w3.org/2011/webrtc/editor/webrtc.html. The API is
    still evolving, but the basic principles of this test should hold true.

    The test will bring up webrtc_test.html in two tabs and tell the web pages
    to start up WebRTC, which will acquire video and audio devices on the
    system. This will launch a dialog in Chrome which we click past using the
    automation controller. Then, we will order both tabs to connect the server,
    which will make the two tabs aware of each other. Once that is done we order
    one tab to call the other. We make sure that the javascript tells us that
    the call succeeded, let it run for a while and try to hang up the call
    after that.
    """
    assert self.IsLinux()

    url = self.GetFileURLForDataPath('webrtc', 'webrtc_test.html')
    self.NavigateToURL(url)
    self.AppendTab(pyauto.GURL(url))

    self.assertEquals('ok-got-stream', self._GetUserMedia(tab_index=0))
    self.assertEquals('ok-got-stream', self._GetUserMedia(tab_index=1))
    self._Connect("user_1", tab_index=0)
    self._Connect("user_2", tab_index=1)

    self._EstablishCall(from_tab_with_index=0)

    # Give the call some time to run so video flows through the system.
    time.sleep(5)

    # The hang-up will automatically propagate to the second tab.
    self._HangUp(from_tab_with_index=0)
    self._VerifyHungUp(tab_index=1)

    # Ensure we didn't miss any errors.
    self._AssertNoFailuresReceivedInTwoTabs()

  def testHandlesNewGetUserMediaRequestSeparately(self):
    """Ensures WebRTC doesn't allow new requests to piggy-back on old ones."""
    assert self.IsLinux()

    url = self.GetFileURLForDataPath('webrtc', 'webrtc_test.html')
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

    result = self.ExecuteJavascript(
        'obtainGetUserMediaResult()', tab_index=tab_index)
    self._AssertNoFailuresReceivedInTwoTabs()
    return result

  def _Connect(self, user_name, tab_index):
    self.assertEquals('ok-connected', self.ExecuteJavascript(
        'connect("http://localhost:8888", "%s")' % user_name,
        tab_index=tab_index))
    self._AssertNoFailuresReceivedInTwoTabs()

  def _EstablishCall(self, from_tab_with_index):
    self.assertEquals('ok-call-established', self.ExecuteJavascript(
        'call()', tab_index=from_tab_with_index))
    self._AssertNoFailuresReceivedInTwoTabs()

    # Double-check the call reached the other side.
    self.assertEquals('yes', self.ExecuteJavascript(
        'is_call_active()', tab_index=from_tab_with_index))

  def _HangUp(self, from_tab_with_index):
    self.assertEquals('ok-call-hung-up', self.ExecuteJavascript(
        'hangUp()', tab_index=from_tab_with_index))
    self._VerifyHungUp(from_tab_with_index)
    self._AssertNoFailuresReceivedInTwoTabs()

  def _VerifyHungUp(self, tab_index):
    self.assertEquals('no', self.ExecuteJavascript(
        'is_call_active()', tab_index=tab_index))

  def _AssertNoFailuresReceivedInTwoTabs(self):
    # Make sure both tabs' errors get reported if there is a problem.
    tab_0_errors = self.ExecuteJavascript('getAnyTestFailures()', tab_index=0)
    tab_1_errors = self.ExecuteJavascript('getAnyTestFailures()', tab_index=1)

    result = 'Tab 0: %s Tab 1: %s' % (tab_0_errors, tab_1_errors)

    self.assertEquals('Tab 0: ok-no-errors Tab 1: ok-no-errors', result)


if __name__ == '__main__':
  pyauto_functional.Main()
