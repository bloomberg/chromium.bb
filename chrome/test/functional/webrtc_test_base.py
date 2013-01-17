#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess

import pyauto


class MissingRequiredBinaryException(Exception):
  pass


class WebrtcTestBase(pyauto.PyUITest):
  """This base class provides helpers for WebRTC calls."""

  def ExtraChromeFlags(self):
    """Adds flags to the Chrome command line."""
    extra_flags = ['--enable-data-channels', '--enable-dcheck']
    return pyauto.PyUITest.ExtraChromeFlags(self) + extra_flags

  def GetUserMedia(self, tab_index, action='accept',
                   request_video=True, request_audio=True):
    """Acquires webcam or mic for one tab and returns the result.

    Args:
      tab_index: The tab to request user media on.
      action: The action to take on the info bar. Can be 'accept', 'cancel' or
          'dismiss'.
      request_video: Whether to request video.
      request_audio: Whether to request audio.

    Returns:
      A string as specified by the getUserMedia javascript function.
    """
    constraints = '{ video: %s, audio: %s }' % (str(request_video).lower(),
                                                str(request_audio).lower())
    self.assertEquals('ok-requested', self.ExecuteJavascript(
        'getUserMedia("%s")' % constraints, tab_index=tab_index))

    self.WaitForInfobarCount(1, tab_index=tab_index)
    self.PerformActionOnInfobar(action, infobar_index=0, tab_index=tab_index)
    self.WaitForGetUserMediaResult(tab_index=0)

    result = self.GetUserMediaResult(tab_index=0)
    self.AssertNoFailures(tab_index)
    return result

  def WaitForGetUserMediaResult(self, tab_index):
    """Waits until WebRTC has responded to a getUserMedia query.

    Fails an assert if WebRTC doesn't respond within the default timeout.

    Args:
      tab_index: the tab to query.
    """
    def HasResult():
      return self.GetUserMediaResult(tab_index) != 'not-called-yet'
    self.assertTrue(self.WaitUntil(HasResult),
                    msg='Timed out while waiting for getUserMedia callback.')

  def GetUserMediaResult(self, tab_index):
    """Retrieves WebRTC's answer to a user media query.

    Args:
      tab_index: the tab to query.

    Returns:
      Specified in obtainGetUserMediaResult() in getusermedia.js.
    """
    return self.ExecuteJavascript(
        'obtainGetUserMediaResult()', tab_index=tab_index)

  def AssertNoFailures(self, tab_index):
    """Ensures the javascript hasn't registered any asynchronous errors.

    Args:
      tab_index: The tab to check.
    """
    self.assertEquals('ok-no-errors', self.ExecuteJavascript(
        'getAnyTestFailures()', tab_index=tab_index))

  def Connect(self, user_name, tab_index):
    self.assertEquals('ok-connected', self.ExecuteJavascript(
        'connect("http://localhost:8888", "%s")' % user_name,
        tab_index=tab_index))
    self.AssertNoFailures(tab_index)

  def EstablishCall(self, from_tab_with_index, to_tab_with_index):
    self.WaitUntilPeerConnects(tab_index=from_tab_with_index)

    self.assertEquals('ok-peerconnection-created', self.ExecuteJavascript(
        'preparePeerConnection()', tab_index=from_tab_with_index))
    self.AssertNoFailures(from_tab_with_index)

    self.assertEquals('ok-added', self.ExecuteJavascript(
        'addLocalStream()', tab_index=from_tab_with_index))
    self.AssertNoFailures(from_tab_with_index)

    self.assertEquals('ok-negotiating', self.ExecuteJavascript(
        'negotiateCall()', tab_index=from_tab_with_index))
    self.AssertNoFailures(from_tab_with_index)

    self.WaitUntilReadyState(ready_state='active',
                             tab_index=from_tab_with_index)

    # Double-check the call reached the other side.
    self.WaitUntilReadyState(ready_state='active',
                             tab_index=to_tab_with_index)

  def HangUp(self, from_tab_with_index):
    self.assertEquals('ok-call-hung-up', self.ExecuteJavascript(
        'hangUp()', tab_index=from_tab_with_index))
    self.WaitUntilHangUpVerified(tab_index=from_tab_with_index)
    self.AssertNoFailures(tab_index=from_tab_with_index)

  def WaitUntilPeerConnects(self, tab_index):
    peer_connected = self.WaitUntil(
        function=lambda: self.ExecuteJavascript('remotePeerIsConnected()',
                                                tab_index=tab_index),
        expect_retval='peer-connected')
    self.assertTrue(peer_connected,
                    msg='Timed out while waiting for peer to connect.')

  def WaitUntilReadyState(self, ready_state, tab_index):
    got_ready_state = self.WaitUntil(
        function=lambda: self.ExecuteJavascript('getPeerConnectionReadyState()',
                                                tab_index=tab_index),
        expect_retval=ready_state)
    self.assertTrue(got_ready_state,
                    msg=('Timed out while waiting for peer connection ready '
                         'state to change to %s for tab %d.' % (ready_state,
                                                                tab_index)))

  def WaitUntilHangUpVerified(self, tab_index):
    self.WaitUntilReadyState('no-peer-connection', tab_index=tab_index)

  def Disconnect(self, tab_index):
    self.assertEquals('ok-disconnected', self.ExecuteJavascript(
        'disconnect()', tab_index=tab_index))

  def BinPathForPlatform(self, path):
    """Form a platform specific path to a binary.

    Args:
      path(string): The path to the binary without an extension.
    Return:
      (string): The platform-specific bin path.
    """
    if self.IsWin():
      path += '.exe'
    return path

  def StartPeerConnectionServer(self):
    """Starts peerconnection_server.

    Peerconnection_server is a custom binary allowing two WebRTC clients to find
    each other. For more details, see the source code which is available at the
    site http://code.google.com/p/libjingle/source/browse/ (make sure to browse
    to trunk/talk/examples/peerconnection/server).
    """
    # Start the peerconnection_server. It should be next to chrome.
    binary_path = os.path.join(self.BrowserPath(), 'peerconnection_server')
    binary_path = self.BinPathForPlatform(binary_path)

    if not os.path.exists(binary_path):
      raise MissingRequiredBinaryException(
        'Could not locate peerconnection_server. Have you built the '
        'peerconnection_server target? We expect to have a '
        'peerconnection_server binary next to the chrome binary.')

    self._server_process = subprocess.Popen(binary_path)

  def StopPeerConnectionServer(self):
    """Stops the peerconnection_server."""
    assert self._server_process
    self._server_process.kill()
