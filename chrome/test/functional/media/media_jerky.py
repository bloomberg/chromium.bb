#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple test for HTML5 media tag to measure playback time.

This PyAuto powered script plays media (video or audio) files using the HTML5
tag embedded in an HTML file (specified in the GetPlayerHTMLFileName() method)
for a test that uses a jerky tool that is being developed. The tool will measure
display Frame-per-second (FPS) (instead of decode FPS, which is measured in
media_fps.py). The tool requires predefined pattern to show before video plays
to determine the area to be measured (it is done in
data/media/html/media_jerky.html).

The parameters needed to run this test are passed in the form of environment
variables (such as the number of runs). Media_perf_runner.py is used for
generating these variables (PyAuto does not support direct parameters).
"""
import os
import time

from media_test_base import MediaTestBase
from media_test_env_names import MediaTestEnvNames
import pyauto_media


class MediaPlaybackTimeTest(MediaTestBase):
  """Test class to record playback time."""

  def testHTML5MediaTag(self):
    """Test the HTML5 media tag."""
    MediaTestBase.ExecuteTest(self)

  def PreAllRunsProcess(self):
    """A method to be executed before all runs."""
    os.environ[MediaTestEnvNames.JERKY_TEST_ENV_NAME] = 'True'
    MediaTestBase.PreAllRunsProcess(self)

  def GetPlayerHTMLFileName(self):
    """A method to get the player HTML file name."""
    return 'media_jerky.html'


if __name__ == '__main__':
  pyauto_media.Main()
