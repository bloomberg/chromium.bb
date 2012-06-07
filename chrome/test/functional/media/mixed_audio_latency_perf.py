#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Audio latency performance test.

Benchmark measuring how fast we can continuously repeat a short sound
clip, while another clip is running in the background. In the ideal
scenario we'd have zero latency processing script, seeking back to the
beginning of the clip, and resuming audio playback.

Performance is recorded as the average latency of N playbacks.  I.e., if we play
a clip 50 times and the ideal duration of all playbacks is 1000ms and the total
duration is 1500ms, the recorded result is (1500ms - 1000ms) / 50 == 10ms.
"""
import os

import pyauto_media
import pyauto_utils
import pyauto


# HTML test path; relative to src/chrome/test/data.
_TEST_HTML_PATH = os.path.join('media', 'html', 'mixed_audio_latency_perf.html')


class MixedAudioLatencyPerfTest(pyauto.PyUITest):
  """PyAuto test container.  See file doc string for more information."""

  def testAudioLatency(self):
    """Launches HTML test which runs the audio latency test."""
    self.NavigateToURL(self.GetFileURLForDataPath(_TEST_HTML_PATH))

    # Block until the test finishes and notifies us.
    self.assertTrue(self.ExecuteJavascript('startTest();'))
    latency = float(self.GetDOMValue('averageLatency'))
    pyauto_utils.PrintPerfResult('audio_latency', 'latency_bg_clip', latency,
                                 'ms')


if __name__ == '__main__':
  pyauto_media.Main()
