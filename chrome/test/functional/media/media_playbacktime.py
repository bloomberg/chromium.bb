#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple test for HTML5 media tag to measure playback time.

This PyAuto powered script plays media (video or audio) files using the HTML5
tag embedded in an HTML file (specified in the GetPlayerHTMLFileName() method)
and measures simple playback time. The parameters needed to run this test are
passed in the form of environment variables (such as the number of runs).
media_perf_runner.py is used for generating these variables
(PyAuto does not support direct parameters).
"""

import pyauto_media
from media_test_base import MediaTestBase


class MediaPlaybackTimeTest(MediaTestBase):
  """Test class to record playback time."""

  def testHTML5MediaTag(self):
    """Test the HTML5 media tag."""
    MediaTestBase.ExecuteTest(self)


if __name__ == '__main__':
  pyauto_media.Main()
