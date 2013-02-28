#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import tempfile
import time

import media.audio_tools as audio_tools

# Note: pyauto_functional must come before pyauto.
import pyauto_functional
import pyauto
import webrtc_test_base

class WebrtcAudioCallTest(webrtc_test_base.WebrtcTestBase):
  """Test we can set up a WebRTC call and play audio through it."""
  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self.StartPeerConnectionServer()

  def tearDown(self):
    self.StopPeerConnectionServer()

    pyauto.PyUITest.tearDown(self)
    self.assertEquals('', self.CheckErrorsAndCrashes())

  def testWebrtcAudioCallAndVerifyAudioIsPlaying(self):
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
    self.assertTrue(self.IsLinux(), msg='Only supported on Linux.')

    def CallWithAudio():
      self._RunWebrtcCall(duration_seconds=5)

    self._RecordAudioAndEnsureNotSilent(record_duration_seconds=10,
                                        sound_producing_function=CallWithAudio)

  def _RunWebrtcCall(self, duration_seconds):
    self.LoadTestPageInTwoTabs()

    # This sets up a audio-only call.
    self.assertEquals('ok-got-stream', self.GetUserMedia(tab_index=0,
                                                         request_video=False))
    self.assertEquals('ok-got-stream', self.GetUserMedia(tab_index=1,
                                                         request_video=False))
    self.Connect('user_1', tab_index=0)
    self.Connect('user_2', tab_index=1)

    self.EstablishCall(from_tab_with_index=0, to_tab_with_index=1)

    # Keep the call up while we detect audio.
    time.sleep(duration_seconds)

    # The hang-up will automatically propagate to the second tab.
    self.HangUp(from_tab_with_index=0)
    self.WaitUntilHangUpVerified(tab_index=1)

    self.Disconnect(tab_index=0)
    self.Disconnect(tab_index=1)

    # Ensure we didn't miss any errors.
    self.AssertNoFailures(tab_index=0)
    self.AssertNoFailures(tab_index=1)

  def _RecordAudioAndEnsureNotSilent(self, record_duration_seconds,
                                     sound_producing_function):
    _SIZE_OF_EMPTY_DAT_FILE_BYTES = 44

    # The two temp files that will be potentially used in the test.
    temp_file = None
    file_no_silence = None
    try:
      temp_file = self._CreateTempFile()
      record_thread = audio_tools.AudioRecorderThread(record_duration_seconds,
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