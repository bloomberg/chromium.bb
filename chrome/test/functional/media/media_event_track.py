#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple event test for the HTML5 media tag.

This PyAuto powered script plays media (video or audio) files using the HTML5
tag embedded in an HTML file (specified in the GetPlayerHTMLFileName() method)
and asserts proper event occurrence relating to track (caption). The parameters
needed to run this test are passed in the form of environment variables
(such as the number of runs). Media_perf_runner.py is used for generating
these variables (PyAuto does not support direct parameters).
"""

import os

import pyauto_media
from media_event_test_base import MediaEventTestBase
from media_test_env_names import MediaTestEnvNames


class MediaEventTrackSimpleTest(MediaEventTestBase):
  """Tests for simple media events."""

  def testHTML5MediaTag(self):
    """Test the HTML5 media tag."""
    track_filename = os.getenv(MediaTestEnvNames.TRACK_FILE_ENV_NAME, '')
    # Override the default test when track is used.
    # There are two types of events listed here:
    #   0: event occurrence is 0.
    #   None: event occurrence is more than 1.
    # TODO(imasaki@chromium.org): uncomment below when we enable events.
    if track_filename:
      # self.event_expected_values['cuechange'] = None
      # self.event_expected_values['enter'] = None
      # self.event_expected_values['exit'] = None
      # self.event_expected_values['change'] = None
      pass
    MediaEventTestBase.ExecuteTest(self)


if __name__ == '__main__':
  pyauto_media.Main()
