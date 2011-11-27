#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple test for HTML5 media tag to test track (caption) functionality.

This PyAuto powered script plays media (video or audio) files using the HTML5
tag embedded in an HTML file (specified in the GetPlayerHTMLFileName() method)
and tests track (caption) fuctionality. The parameters needed to run this test
are passed in the form of environment variables (such as the number of runs).
media_perf_runner.py is used for generating these variables
(PyAuto does not support direct parameters).
"""
import os
import time

from media_test_base import MediaTestBase
from media_test_env_names import MediaTestEnvNames
import pyauto_media
from ui_perf_test_utils import UIPerfTestUtils


class MediaPlaybackTimeTest(MediaTestBase):
  """Test class to record playback time."""

  def testHTML5MediaTag(self):
    """Test the HTML5 media tag."""
    MediaTestBase.ExecuteTest(self)

  def PostAllRunsProcess(self):
    """A method to execute after all runs.

    This is an overridden method. Currently, noop.
    """
    pass

  def PostEachRunProcess(self, unused_run_counter):
    """A method to execute after each run.

    This is an overridden method. Currently noop.

    Args:
      unused_run_counter: counter for each run.
    """
    pass

  def GetPlayerHTMLFileName(self):
    """A method to get the player HTML file name."""
    return 'media_track.html'


if __name__ == '__main__':
  pyauto_media.Main()
