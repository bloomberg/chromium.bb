#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple test for HTML5 media tag to measure playback time.

This PyAuto powered script plays media (video or audio) files (using HTML5 tag
embedded in player.html file) and records whole playback time. The parameters
needed to run this performance test are passed in the form of environment
variables (such as the number of runs). media_perf_runner.py is used for
generating these variables (PyAuto does not support direct parameters).
"""
import pyauto_functional  # Must be imported before pyauto.
import pyauto

from media_test_base import MediaTestBase


class MediaPlaybackTimeTest(MediaTestBase):
  """Test class to record playback time."""

  def testHTML5MediaTag(self):
    """test HTML5 Media Tag."""
    MediaTestBase.ExecuteTest(self)


if __name__ == '__main__':
  pyauto_functional.Main()
