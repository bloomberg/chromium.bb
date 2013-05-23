#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Audio tools for recording and analyzing audio.

The audio tools provided here are mainly to:
- record playing audio.
- remove silence from beginning and end of audio file.
- compare audio files using PESQ tool.

The tools are supported on Windows and Linux.
"""

import commands
import ctypes
import logging
import os
import re
import subprocess
import sys
import threading
import time

import pyauto_media
import pyauto


_TOOLS_PATH = os.path.abspath(os.path.join(pyauto.PyUITest.DataDir(),
                                           'pyauto_private', 'media', 'tools'))

WINDOWS = 'win32' in sys.platform
if WINDOWS:
  _PESQ_PATH = os.path.join(_TOOLS_PATH, 'pesq.exe')
  _SOX_PATH = os.path.join(_TOOLS_PATH, 'sox.exe')
  _AUDIO_RECORDER = r'SoundRecorder.exe'
  _FORCE_MIC_VOLUME_MAX_UTIL = os.path.join(_TOOLS_PATH,
                                            r'force_mic_volume_max.exe')
else:
  _PESQ_PATH = os.path.join(_TOOLS_PATH, 'pesq')
  _SOX_PATH = commands.getoutput('which sox')
  _AUDIO_RECORDER = commands.getoutput('which arecord')
  _PACMD_PATH = commands.getoutput('which pacmd')


class AudioRecorderThread(threading.Thread):
  """A thread that records audio out of the default audio output."""

  def __init__(self, duration, output_file, record_mono=False):
    threading.Thread.__init__(self)
    self.error = ''
    self._duration = duration
    self._output_file = output_file
    self._record_mono = record_mono

  def run(self):
    """Starts audio recording."""
    if WINDOWS:
      if self._record_mono:
        logging.error("Mono recording not supported on Windows yet!")

      duration = time.strftime('%H:%M:%S', time.gmtime(self._duration))
      cmd = [_AUDIO_RECORDER, '/FILE', self._output_file, '/DURATION',
             duration]
      # This is needed to run SoundRecorder.exe on Win-64 using Python-32 bit.
      ctypes.windll.kernel32.Wow64DisableWow64FsRedirection(
          ctypes.byref(ctypes.c_long()))
    else:
      num_channels = 1 if self._record_mono else 2
      cmd = [_AUDIO_RECORDER, '-d', self._duration, '-f', 'dat', '-c',
             str(num_channels), self._output_file]

    cmd = [str(s) for s in cmd]
    logging.debug('Running command: %s', ' '.join(cmd))
    returncode = subprocess.call(cmd, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)
    if returncode != 0:
      self.error = 'Failed to record audio.'
    else:
      logging.debug('Finished recording audio into %s.', self._output_file)


def RunPESQ(audio_file_ref, audio_file_test, sample_rate=16000):
  """Runs PESQ to compare audio test file to a reference audio file.

  Args:
    audio_file_ref: The reference audio file used by PESQ.
    audio_file_test: The audio test file to compare.
    sample_rate: Sample rate used by PESQ algorithm, possible values are only
        8000 or 16000.

  Returns:
    A tuple of float values representing PESQ scores of the audio_file_ref and
    audio_file_test consecutively.
  """
  # Work around a bug in PESQ when the ref file path is > 128 chars. PESQ will
  # compute an incorrect score then (!), and the relative path to the ref file
  # should be a lot shorter than the absolute one.
  audio_file_ref = os.path.relpath(audio_file_ref)
  cmd = [_PESQ_PATH, '+%d' % sample_rate, audio_file_ref, audio_file_test]
  logging.debug('Running command: %s', ' '.join(cmd))
  p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  output, error = p.communicate()
  if p.returncode != 0:
    logging.error('Error running pesq: %s\n%s', output, error)
  # Last line of PESQ output shows the results.  Example:
  # P.862 Prediction (Raw MOS, MOS-LQO):  = 4.180    4.319
  result = re.search('Prediction.*= (\d{1}\.\d{3})\t(\d{1}\.\d{3})',
                     output)
  if not result or len(result.groups()) != 2:
    return None
  return (float(result.group(1)), float(result.group(2)))


def RemoveSilence(input_audio_file, output_audio_file):
  """Removes silence from beginning and end of the input_audio_file.

  Args:
    input_audio_file: The audio file to remove silence from.
    output_audio_file: The audio file to save the output audio.
  """
  # SOX documentation for silence command: http://sox.sourceforge.net/sox.html
  # To remove the silence from both beginning and end of the audio file, we call
  # sox silence command twice: once on normal file and again on its reverse,
  # then we reverse the final output.
  # Silence parameters are (in sequence):
  # ABOVE_PERIODS: The period for which silence occurs. Value 1 is used for
  #                 silence at beginning of audio.
  # DURATION: the amount of time in seconds that non-silence must be detected
  #           before sox stops trimming audio.
  # THRESHOLD: value used to indicate what sample value is treates as silence.
  ABOVE_PERIODS = '1'
  DURATION = '2'
  THRESHOLD = '5%'

  cmd = [_SOX_PATH, input_audio_file, output_audio_file, 'silence',
         ABOVE_PERIODS, DURATION, THRESHOLD, 'reverse', 'silence',
         ABOVE_PERIODS, DURATION, THRESHOLD, 'reverse']
  logging.debug('Running command: %s', ' '.join(cmd))
  p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  output, error = p.communicate()
  if p.returncode != 0:
    logging.error('Error removing silence from audio: %s\n%s', output, error)


def ForceMicrophoneVolumeTo100Percent():
  if WINDOWS:
    # The volume max util is implemented in WebRTC in
    # webrtc/tools/force_mic_volume_max/force_mic_volume_max.cc.
    if not os.path.exists(_FORCE_MIC_VOLUME_MAX_UTIL):
      raise Exception('Missing required binary %s.' %
                      _FORCE_MIC_VOLUME_MAX_UTIL)
    cmd = [_FORCE_MIC_VOLUME_MAX_UTIL]
  else:
    # The recording device id is machine-specific. We assume here it is called
    # Monitor of render (which corresponds to the id render.monitor). You can
    # list the available recording devices with pacmd list-sources.
    RECORDING_DEVICE_ID = 'render.monitor'
    HUNDRED_PERCENT_VOLUME = '65536'
    cmd = [_PACMD_PATH, 'set-source-volume', RECORDING_DEVICE_ID,
           HUNDRED_PERCENT_VOLUME]

  logging.debug('Running command: %s', ' '.join(cmd))
  p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  output, error = p.communicate()
  if p.returncode != 0:
    logging.error('Error forcing mic volume to 100%%: %s\n%s', output, error)
