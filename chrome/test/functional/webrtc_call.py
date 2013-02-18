#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import tempfile
import time

import media.audio_tools as audio_tools

# This little construct ensures we can run even if we have a bad version of
# psutil installed. If so, we'll just skip the test that needs it.
_HAS_CORRECT_PSUTIL_VERSION = False
try:
  import psutil
  if 'version_info' in dir(psutil):
    # If psutil has any version info at all, it's recent enough.
    _HAS_CORRECT_PSUTIL_VERSION = True
except ImportError, e:
  pass


# Note: pyauto_functional must come before pyauto.
import pyauto_functional
import pyauto
import pyauto_utils
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

  def _SimpleWebrtcCall(self, request_video, request_audio, duration_seconds=0):
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

    Args:
      request_video: Whether to request video.
      request_audio: Whether to request audio.
      duration_seconds: The number of seconds to keep the call up before
        shutting it down.
    """
    self._SetupCall(request_video=request_video, request_audio=request_audio)

    if duration_seconds:
      print 'Call up: sleeping %d seconds...' % duration_seconds
      time.sleep(duration_seconds);

    # The hang-up will automatically propagate to the second tab.
    self.HangUp(from_tab_with_index=0)
    self.WaitUntilHangUpVerified(tab_index=1)

    self.Disconnect(tab_index=0)
    self.Disconnect(tab_index=1)

    # Ensure we didn't miss any errors.
    self.AssertNoFailures(tab_index=0)
    self.AssertNoFailures(tab_index=1)

  def testWebrtcJsep01Call(self):
    """Uses a draft of the PeerConnection API, using JSEP01."""
    self._LoadPageInTwoTabs('webrtc_jsep01_test.html')
    self._SimpleWebrtcCall(request_video=True, request_audio=True)

  def testWebrtcVideoOnlyJsep01Call(self):
    self._LoadPageInTwoTabs('webrtc_jsep01_test.html')
    self._SimpleWebrtcCall(request_video=True, request_audio=False)

  def testWebrtcAudioOnlyJsep01Call(self):
    self._LoadPageInTwoTabs('webrtc_jsep01_test.html')
    self._SimpleWebrtcCall(request_video=False, request_audio=True)

  def testWebrtcJsep01CallAndVerifyAudioIsPlaying(self):
    """Test that WebRTC is capable of transmitting at least some audio.

    This test has some nontrivial prerequisites:
    1. The target system must have an active microphone, it must be selected
       as default input for the user that runs the test, and it must record a
       certain minimum level of ambient noise (for instance server fans).
       Verify that you are getting ambient noise in the microphone by either
       recording it directly or checking your OS' microphone settings. Amplify
       the microphone if the background noise is too low. The microphone should
       capture noise consistently above 5% of its total range.
    2. The target system must be configured to record its own input*.

    * On Linux:
    1. # sudo apt-get install pavucontrol
    2. For the user who will run the test: # pavucontrol
    3. In a separate terminal, # arecord dummy
    4. In pavucontrol, go to the recording tab.
    5. For the ALSA plug-in [aplay]: ALSA Capture from, change from <x> to
       <Monitor of x>, where x is whatever your primary sound device is called.
    6. Try launching chrome as the target user on the target machine, try
       playing, say, a YouTube video, and record with # arecord -f dat mine.dat.
       Verify the recording with aplay (should have recorded what you played
       from chrome).
    """
    if not self.IsLinux():
      print 'This test is only available on Linux for now.'
      return

    self._LoadPageInTwoTabs('webrtc_jsep01_test.html')

    def AudioCall():
      self._SimpleWebrtcCall(request_video=False, request_audio=True,
                             duration_seconds=5)
    self._RecordAudioAndEnsureNotSilent(record_duration_seconds=10,
                                        sound_producing_function=AudioCall)

  def testJsep01AndMeasureCpu20Seconds(self):
    if not _HAS_CORRECT_PSUTIL_VERSION:
      print ('WARNING: Can not run cpu/mem measurements with this version of '
             'psutil. You must have at least psutil 0.4.1 installed for the '
             'version of python you are running this test with.')
      return

    self._LoadPageInTwoTabs('webrtc_jsep01_test.html')

    # Prepare CPU measurements.
    renderer_process = self._GetChromeRendererProcess(tab_index=0)
    renderer_process.get_cpu_percent()

    self._SimpleWebrtcCall(request_video=True,
                           request_audio=True,
                           duration_seconds=20)

    cpu_usage = renderer_process.get_cpu_percent(interval=0)
    mem_usage_bytes = renderer_process.get_memory_info()[0]
    mem_usage_kb = float(mem_usage_bytes) / 1024
    pyauto_utils.PrintPerfResult('cpu', 'jsep01_call', cpu_usage, '%')
    pyauto_utils.PrintPerfResult('memory', 'jsep01_call', mem_usage_kb, 'KiB')

  def testLocalPreview(self):
    """Brings up a local preview and ensures video is playing.

    This test will launch a window with a single tab and run a getUserMedia call
    which will give us access to the webcam and microphone. Then the javascript
    code will hook up the webcam data to the local-view video tag. We will
    detect video in that tag using the video detector, and if we see video
    moving the test passes.
    """
    url = self.GetFileURLForDataPath('webrtc', 'webrtc_jsep01_test.html')
    self.NavigateToURL(url)
    self.assertEquals('ok-got-stream', self.GetUserMedia(tab_index=0))
    self._StartDetectingVideo(tab_index=0, video_element='local-view')

    self._WaitForVideo(tab_index=0, expect_playing=True)

  def testHandlesNewGetUserMediaRequestSeparately(self):
    """Ensures WebRTC doesn't allow new requests to piggy-back on old ones."""
    url = self.GetFileURLForDataPath('webrtc', 'webrtc_jsep01_test.html')
    self.NavigateToURL(url)
    self.AppendTab(pyauto.GURL(url))

    self.GetUserMedia(tab_index=0)
    self.GetUserMedia(tab_index=1)
    self.Connect("user_1", tab_index=0)
    self.Connect("user_2", tab_index=1)

    self.EstablishCall(from_tab_with_index=0, to_tab_with_index=1)

    self.assertEquals('failed-with-error-1',
                      self.GetUserMedia(tab_index=0, action='cancel'))
    self.assertEquals('failed-with-error-1',
                      self.GetUserMedia(tab_index=0, action='dismiss'))

  def testMediaStreamTrackEnable(self):
    """Tests MediaStreamTrack.enable on tracks connected to a PeerConnection.

    This test will check that if a local track is muted, the remote end don't
    get video. Also test that if a remote track is disabled, the video is not
    updated in the video tag."""

    # TODO(perkj): Also verify that the local preview is muted when the
    # feature is implemented.
    # TODO(perkj): Verify that audio is muted.

    self._LoadPageInTwoTabs('webrtc_jsep01_test.html')
    self._SetupCall(request_video=True, request_audio=True)
    select_video_function = \
        'function(local) { return local.getVideoTracks()[0]; }'
    self.assertEquals('ok-video-toggled-to-false', self.ExecuteJavascript(
        'toggleLocalStream(' + select_video_function + ', "video")',
        tab_index=0))
    self._WaitForVideo(tab_index=1, expect_playing=False)

    self.assertEquals('ok-video-toggled-to-true', self.ExecuteJavascript(
        'toggleLocalStream(' + select_video_function + ', "video")',
        tab_index=0))
    self._WaitForVideo(tab_index=1, expect_playing=True)

    # Test disabling a remote stream. The remote video is not played."""
    self.assertEquals('ok-video-toggled-to-false', self.ExecuteJavascript(
        'toggleRemoteStream(' + select_video_function + ', "video")',
        tab_index=1))
    self._WaitForVideo(tab_index=1, expect_playing=False)

    self.assertEquals('ok-video-toggled-to-true', self.ExecuteJavascript(
        'toggleRemoteStream(' + select_video_function + ', "video")',
        tab_index=1))
    self._WaitForVideo(tab_index=1, expect_playing=True)

  def _LoadPageInTwoTabs(self, test_page):
    url = self.GetFileURLForDataPath('webrtc', test_page)
    self.NavigateToURL(url)
    self.AppendTab(pyauto.GURL(url))

  def _SetupCall(self, request_video, request_audio):
    """Gets user media and establishes a call.

    Assumes that two tabs are already opened with a suitable test page.

    Args:
      request_video: Whether to request video.
      request_audio: Whether to request audio.
    """
    self.assertEquals('ok-got-stream', self.GetUserMedia(
        tab_index=0, request_video=request_video, request_audio=request_audio))
    self.assertEquals('ok-got-stream', self.GetUserMedia(
        tab_index=1, request_video=request_video, request_audio=request_audio))
    self.Connect('user_1', tab_index=0)
    self.Connect('user_2', tab_index=1)

    self.EstablishCall(from_tab_with_index=0, to_tab_with_index=1)

    if request_video:
      self._StartDetectingVideo(tab_index=0, video_element='remote-view')
      self._StartDetectingVideo(tab_index=1, video_element='remote-view')

      self._WaitForVideo(tab_index=0, expect_playing=True)
      self._WaitForVideo(tab_index=1, expect_playing=True)

  def _StartDetectingVideo(self, tab_index, video_element):
    self.assertEquals('ok-started', self.ExecuteJavascript(
        'startDetection("%s", "frame-buffer", 320, 240)' % video_element,
        tab_index=tab_index));

  def _WaitForVideo(self, tab_index, expect_playing):
    # TODO(phoglund): Remove this hack if we manage to get a more stable Linux
    # bot to run these tests.
    if self.IsLinux():
      print "Linux; pretending to wait for video..."
      time.sleep(1)
      return

    expect_retval='video-playing' if expect_playing else 'video-not-playing'

    video_playing = self.WaitUntil(
        function=lambda: self.ExecuteJavascript('isVideoPlaying()',
                                                tab_index=tab_index),
        expect_retval=expect_retval)
    self.assertTrue(video_playing,
                    msg= 'Timed out while waiting for isVideoPlaying to ' +
                         'return ' + expect_retval + '.')

  def _GetChromeRendererProcess(self, tab_index):
    """Returns the Chrome renderer process as a psutil process wrapper."""
    tab_info = self.GetBrowserInfo()['windows'][0]['tabs'][tab_index]
    renderer_id = tab_info['renderer_pid']
    if not renderer_id:
      self.fail('Can not find the tab renderer process.')
    return psutil.Process(renderer_id)

  def _RecordAudioAndEnsureNotSilent(self, record_duration_seconds,
                                     sound_producing_function):
    _RECORD_DURATION = 10
    _SIZE_OF_EMPTY_DAT_FILE_BYTES = 44

    # The two temp files that will be potentially used in the test.
    temp_file = None
    file_no_silence = None
    try:
      temp_file = self._CreateTempFile()
      record_thread = audio_tools.AudioRecorderThread(_RECORD_DURATION,
                                                      temp_file)
      record_thread.start()
      sound_producing_function()
      record_thread.join()

      if record_thread.error:
        self.fail(record_thread.error)
      file_no_silence = self._CreateTempFile()
      audio_tools.RemoveSilence(temp_file, file_no_silence)

      self.assertTrue(os.path.getsize(file_no_silence) >
                      _SIZE_OF_EMPTY_DAT_FILE_BYTES,
                      msg=('The test recorded only silence. Ensure your '
                           'machine is correctly configured for this test.'))
    finally:
      # Delete the temporary files used by the test.
      if temp_file:
        os.remove(temp_file)
      if file_no_silence:
        os.remove(file_no_silence)

  def _CreateTempFile(self):
    """Returns an absolute path to an empty temp file."""
    file_handle, path = tempfile.mkstemp(suffix='_webrtc.dat')
    os.close(file_handle)
    return path


if __name__ == '__main__':
  pyauto_functional.Main()
