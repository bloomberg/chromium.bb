#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Scrubbing performance test for <video>.

Measures the times to scrub video and audio files. Scrubbing is simulated by
having consecutive small seeks performed. The test measures both forward and
backward scrubbing.
"""

import os

import pyauto_media
import pyauto_utils
import pyauto

# HTML test path; relative to src/chrome/test/data.
_TEST_HTML_PATH = os.path.join('media', 'html', 'media_scrub.html')

# Path under data path for test files.
_TEST_MEDIA_PATH = os.path.join('media', 'avperf')

# The media files used for testing.
_TEST_MEDIA = [os.path.join('tulip', name) for name in
               ['tulip2.webm', 'tulip2.wav', 'tulip2.ogv', 'tulip2.ogg',
                'tulip2.mp4', 'tulip2.mp3', 'tulip2.m4a']]


class MediaScrubPerfTest(pyauto.PyUITest):
  """PyAuto test container.  See file doc string for more information."""

  def testMediaScrubPerformance(self):
    """Launches HTML test which runs the scrub test and records performance."""
    self.NavigateToURL(self.GetFileURLForDataPath(_TEST_HTML_PATH))

    for media in _TEST_MEDIA:
      file_name = self.GetFileURLForDataPath(
          os.path.join(_TEST_MEDIA_PATH, media))

      # Some tests take more than the default PyAuto calls timeout, so we start
      # each test and wait until 'testDone' flag is set by the test.
      self.CallJavascriptFunc('startTest', [file_name])

      if not self.WaitUntil(self.GetDOMValue, args=['testDone'],
                            retry_sleep=5, timeout=180, debug=False):
        error_msg = 'Scrubbing tests timed out.'
      else:
        error_msg = self.GetDOMValue('errorMsg')
      if error_msg:
        self.fail('Error while running the test: %s' % error_msg)

      forward_scrub_time = float(self.GetDOMValue('forwardScrubTime'))
      backward_scrub_time = float(self.GetDOMValue('backwardScrubTime'))
      mixed_scrub_time = float(self.GetDOMValue('mixedScrubTime'))
      pyauto_utils.PrintPerfResult('scrubbing', os.path.basename(file_name) +
                                   '_forward', forward_scrub_time, 'ms')
      pyauto_utils.PrintPerfResult('scrubbing', os.path.basename(file_name) +
                                   '_backward', backward_scrub_time, 'ms')
      pyauto_utils.PrintPerfResult('scrubbing', os.path.basename(file_name) +
                                   '_mixed', mixed_scrub_time, 'ms')


if __name__ == '__main__':
  pyauto_media.Main()
