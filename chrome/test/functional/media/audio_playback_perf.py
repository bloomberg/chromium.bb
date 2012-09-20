#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Audio basic playback test.  Verifies basic audio output.

The tests plays an audio file and records its output while playing.  It uses
SOX tool to eliminate silence from begining and end of audio recorded.  Then it
uses PESQ tool to verify the recorded audio against expected file.

The output of PESQ is a pair of numbers between 0 and 5 describing how close the
recorded audio is to its reference file.

The output of this test are the PESQ values that are graphed, example:
RESULT audio_pesq: ref= 4.498 score
RESULT audio_pesq: actual= 4.547 score
"""

import logging
import os
import sys
import tempfile

import audio_tools
import pyauto_media
import pyauto
import pyauto_utils


_TEST_HTML_PATH = pyauto.PyUITest.GetFileURLForDataPath('media', 'html',
                                                        'audio_record.html')
_MEDIA_PATH = os.path.abspath(os.path.join(pyauto.PyUITest.DataDir(),
                                           'pyauto_private', 'media'))

# The media file to play.
_TEST_AUDIO = pyauto.PyUITest.GetFileURLForPath(_MEDIA_PATH, 'dance2_10sec.wav')
# The recording duration should be at least as the media duration.  We use 5
# extra seconds of record in this test.
_RECORD_DURATION = 15

# Due to different noise introduced by audio recording tools on windows and
# linux, we require an expected audio file per platform.
if 'win32' in sys.platform:
  _TEST_EXPECTED_AUDIO = os.path.join(_MEDIA_PATH, 'audio_record_win.wav')
else:
  _TEST_EXPECTED_AUDIO = os.path.join(_MEDIA_PATH, 'audio_record_lin.wav')


def GetTempFilename():
  """Returns an absolute path to an empty temp file."""
  f, path = tempfile.mkstemp(suffix='_actual.wav')
  os.close(f)
  return path


class AudioRecordPerfTest(pyauto.PyUITest):
  """PyAuto test container.  See file doc string for more information."""

  def testBasicAudioPlaybackRecord(self):
    """Plays an audio file and verifies its output against expected."""
    # The 2 temp files that will be potentially used in the test.
    temp_file = None
    file_no_silence = None
    # Rebase test expectation file is REBASE=1 env variable is set.
    rebase = 'REBASE' in os.environ

    try:
      temp_file = GetTempFilename()
      record_thread = audio_tools.AudioRecorderThread(_RECORD_DURATION,
                                                      temp_file)
      record_thread.start()
      self.NavigateToURL(_TEST_HTML_PATH)
      self.assertTrue(self.ExecuteJavascript("startTest('%s');" % _TEST_AUDIO))
      record_thread.join()

      if record_thread.error:
        self.fail(record_thread.error)
      file_no_silence = _TEST_EXPECTED_AUDIO if rebase else GetTempFilename()
      audio_tools.RemoveSilence(temp_file, file_no_silence)

      # Exit if we just want to rebase expected output.
      if rebase:
        return

      pesq_values = audio_tools.RunPESQ(_TEST_EXPECTED_AUDIO, file_no_silence)
      if not pesq_values:
        self.fail('Test failed to get pesq results.')
      pyauto_utils.PrintPerfResult('audio_pesq', 'ref', pesq_values[0], 'score')
      pyauto_utils.PrintPerfResult('audio_pesq', 'actual', pesq_values[1],
                                   'score')
    except:
      logging.error('Test failed: %s', self.GetDOMValue('error'))
    finally:
      # Delete the temporary files used by the test.
      if temp_file:
        os.remove(temp_file)
      if not rebase and file_no_silence:
        os.remove(file_no_silence)


if __name__ == '__main__':
  pyauto_media.Main()
