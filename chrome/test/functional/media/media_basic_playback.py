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
    pyauto.PyUITest.GetFileURLForContentDataPath('media', name)
    for name in ['bear.mp4', 'bear.ogv', 'bear.webm', 'bear_silent.mp4',
                 'bear_silent.ogv', 'bear_silent.webm']]

# Expected events for the first iteration and every iteration thereafter.
_EXPECTED_EVENTS_0 = [('ended', 2), ('playing', 2), ('seeked', 1),
                      ('suspend', 1)]
_EXPECTED_EVENTS_n = [('abort', 1), ('emptied', 1)] + _EXPECTED_EVENTS_0


class MediaBasicPlaybackTest(pyauto.PyUITest):
  """PyAuto test container.  See file doc string for more information."""

  def testBasicPlaybackMatrix(self):
    """Launches HTML test which plays each video until end, seeks, and replays.

    Specifically ensures that after the above sequence of events, the following
    are true:

        1. The first video has only 2x playing, 2x ended, and 1x seeked events.
        2. Each subsequent video additionally has 1x abort and 1x emptied due to
           switching of the src attribute.
        3. video.currentTime == video.duration for each video.

    See the HTML file at _TEST_HTML_PATH for more information.
    """
    self.NavigateToURL(self.GetFileURLForDataPath(_TEST_HTML_PATH))

    for i, media in enumerate(_TEST_VIDEOS):
      logging.debug('Running basic playback test for %s', media)

      # Block until the test finishes and notifies us.  Upon return the value of
      # video.currentTime == video.duration is provided.
      try:
        self.assertTrue(self.ExecuteJavascript("startTest('%s');" % media))

        # PyAuto has trouble with arrays, so convert to string prior to request.
        events = self.GetDOMValue("events.join(',')").split(',')
        counts = [(item, events.count(item)) for item in sorted(set(events))]

        # The first loop will not have the abort and emptied events triggered by
        # changing the video src.
        if (i == 0):
          self.assertEqual(counts, _EXPECTED_EVENTS_0)
        else:
          self.assertEqual(counts, _EXPECTED_EVENTS_n)
      except:
        logging.debug(
            'Test failed with events: %s', self.GetDOMValue("events.join(',')"))
        raise


if __name__ == '__main__':
  pyauto_media.Main()
