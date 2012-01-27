#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Basic playback test.  Checks playback, seek, and replay based on events.

This test uses the bear videos from the test matrix in h264, vp8, and theora
formats.
"""
import logging
import os

import pyauto_media
import pyauto


# HTML test path; relative to src/chrome/test/data.
_TEST_HTML_PATH = os.path.join('media', 'html', 'media_basic_playback.html')

# Test videos to play.  TODO(dalecurtis): Convert to text matrix parser when we
# have more test videos in the matrix.  Code already written, see patch here:
# https://chromiumcodereview.appspot.com/9290008/#ps12
_TEST_VIDEOS = [
    'bear.mp4', 'bear.ogv', 'bear.webm', 'bear_silent.mp4', 'bear_silent.ogv',
    'bear_silent.webm']


class MediaConstrainedNetworkPerfTest(pyauto.PyUITest):
  """PyAuto test container.  See file doc string for more information."""

  def testBasicPlaybackMatrix(self):
    """Launches HTML test which plays a video until end, seeks, and replays.

    Specifically ensures that after the above sequence of events, the following
    is true:

        1. 2x playing, 2x ended, 1x seeked, 0x error, and 0x abort events.
        2. video.currentTime == video.duration.
    """
    for media in _TEST_VIDEOS:
      logging.debug('Running basic playback test for %s', media)

      self.NavigateToURL('%s?media=%s' % (
          self.GetFileURLForDataPath(_TEST_HTML_PATH), media))

      # Block until the test finishes and notifies us.  Upon return the value of
      # video.currentTime == video.duration is provided.
      self.assertTrue(self.ExecuteJavascript('true;'))

      events = self.GetDOMValue('events').split(',')
      counts = [(item, events.count(item)) for item in sorted(set(events))]
      self.assertEqual(counts, [('ended', 2), ('playing', 2), ('seeked', 1)])


if __name__ == '__main__':
  pyauto_media.Main()
