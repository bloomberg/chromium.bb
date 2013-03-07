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
import pyauto_utils
import webrtc_test_base

_MEDIA_PATH = os.path.abspath(os.path.join(pyauto.PyUITest.DataDir(),
                                           'pyauto_private', 'webrtc'))
_REFERENCE_FILE = os.path.join(_MEDIA_PATH, 'human-voice.wav')
_JAVASCRIPT_PATH = os.path.abspath(os.path.join(pyauto.PyUITest.DataDir(),
                                                'webrtc'))


class WebrtcAudioQualityTest(webrtc_test_base.WebrtcTestBase):
  """Test we can set up a WebRTC call and play audio through it.

  This test will only work on machines that have been configured to record their
  own input*.

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

  * On Windows 7:
    1. Control panel > Sound > Manage audio devices.
    2. In the recording tab, right-click in an empty space in the pane with the
       devices. Tick 'show disabled devices'.
    3. You should see a 'stero mix' device - this is what your speakers output.
       Right click > Properties.
    4. In the Listen tab for the mix device, check the 'listen to this device'
       checkbox. Ensure the mix device is the default recording device.
    5. Launch chrome and try playing a video with sound. You should see movement
       in the volume meter for the mix device. Configure the mix device to have
       a level so that it's not too silent (otherwise the silence elimination
       algorithm will delete too much).
  """
  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self.StartPeerConnectionServer()

  def tearDown(self):
    self.StopPeerConnectionServer()

    pyauto.PyUITest.tearDown(self)
    self.assertEquals('', self.CheckErrorsAndCrashes())

  def testWebrtcAudioCallAndMeasureQuality(self):
    """Measures how much WebRTC distorts speech.

    The input file is about 9.3 seconds long and has had silence trimmed on both
    sides. We will set up a WebRTC call, load the file with WebAudio in the
    javascript, connect the WebAudio buffer node to the peer connection and play
    it out on the other side (in a video tag).

    We then record what Chrome plays out. We give it plenty of time to play
    the whole file over the connection, and then we trim silence on both ends.
    That is finally fed into PESQ for comparison.
    """
    # We'll use a relative path since the javascript will be loading the file
    # relative to where the javascript itself is.
    self.assertTrue(os.path.exists(_MEDIA_PATH),
                    msg='Missing pyauto_private in chrome/test/data: you need '
                        'to check out src_internal in your .gclient to run '
                        'this test.')

    input_relative_path = os.path.relpath(_REFERENCE_FILE, _JAVASCRIPT_PATH)

    def CallWithWebAudio():
      self._AudioCallWithWebAudio(duration_seconds=15,
                                  input_relative_path=input_relative_path)

    def MeasureQuality(output_no_silence):
      results = audio_tools.RunPESQ(_REFERENCE_FILE, output_no_silence,
                                    sample_rate=16000)
      self.assertTrue(results, msg=('Failed to compute PESQ (most likely, we '
                                    'recorded only silence)'))
      pyauto_utils.PrintPerfResult('audio_pesq', 'raw_mos', results[0], 'score')
      pyauto_utils.PrintPerfResult('audio_pesq', 'mos_lqo', results[1], 'score')

    self._RecordAndVerify(record_duration_seconds=20,
                          sound_producing_function=CallWithWebAudio,
                          verification_function=MeasureQuality)

  def _AudioCallWithWebAudio(self, duration_seconds, input_relative_path):
    self.LoadTestPageInTwoTabs(test_page='webrtc_audio_quality_test.html');

    self.Connect('user_1', tab_index=0)
    self.Connect('user_2', tab_index=1)

    self.CreatePeerConnection(tab_index=0)
    self.AddWebAudioFile(tab_index=0, input_relative_path=input_relative_path)

    self._CallAndRunFor(duration_seconds)

  def _CallAndRunFor(self, duration_seconds):
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

  def _RecordAndVerify(self, record_duration_seconds, sound_producing_function,
                       verification_function):
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

      verification_function(file_no_silence)
    finally:
      # Delete the temporary files used by the test.
      if temp_file:
        os.remove(temp_file)
      if file_no_silence:
        os.remove(file_no_silence)

  def _CreateTempFile(self):
    """Returns an absolute path to an empty temp file."""
    file_handle, path = tempfile.mkstemp(suffix='_webrtc.wav')
    os.close(file_handle)
    return path


if __name__ == '__main__':
  pyauto_functional.Main()