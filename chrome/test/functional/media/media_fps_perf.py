#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""FPS (frames per second) performance test for <video>.

Calculates decoded fps and dropped fps while playing HTML5 media element. The
test compares results of playing a media file on different video resolutions.
"""

import logging
import os

import pyauto_media
import pyauto
import pyauto_utils

# HTML test path; relative to src/chrome/test/data.
_TEST_HTML_PATH = os.path.join('media', 'html', 'media_fps_perf.html')

# Path under data path for test files.
_TEST_MEDIA_PATH = os.path.join('pyauto_private', 'media', 'crowd')

# The media files used for testing. The map is from the media file type to short
# file names. A perf graph is generated for each file type.
_TEST_VIDEOS = {
    'webm': ['crowd2160', 'crowd1080', 'crowd720', 'crowd480', 'crowd360']
}


def ToMbit(value):
  """Converts a value of byte per sec units into Mbps units."""
  return float(value * 8) / (1024 * 1024)


class MediaFPSPerfTest(pyauto.PyUITest):
  """PyAuto test container.  See file doc string for more information."""

  def testMediaFPSPerformance(self):
    """Launches HTML test which plays each video and records statistics.

    For each video, the test plays till ended event is fired. It records decoded
    fps, dropped fps, and total dropped frames.
    """
    self.NavigateToURL(self.GetFileURLForDataPath(_TEST_HTML_PATH))

    for ext, files in _TEST_VIDEOS.iteritems():
      for name in files:
        file_url = self.GetFileURLForDataPath(
            os.path.join(_TEST_MEDIA_PATH, '%s.%s' % (name, ext)))
        logging.debug('Running fps perf test for %s.', file_url)
        self.assertTrue(self.ExecuteJavascript("startTest('%s');" % file_url))
        decoded_fps = [float(value) for value in
                       self.GetDOMValue("decodedFPS.join(',')").split(',')]
        dropped_frames = self.GetDOMValue('droppedFrames')
        dropped_fps = [float(value) for value in
                       self.GetDOMValue("droppedFPS.join(',')").split(',')]

        pyauto_utils.PrintPerfResult('FPS_' + ext, name, decoded_fps, 'fps')
        pyauto_utils.PrintPerfResult('Dropped_FPS_' + ext, name, dropped_fps,
                                     'fps')
        pyauto_utils.PrintPerfResult('Dropped_Frames_' + ext, name,
                                     dropped_frames, 'frames')


if __name__ == '__main__':
  pyauto_media.Main()
