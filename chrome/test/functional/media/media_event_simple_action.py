#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple event test for the HTML5 media tag.

This PyAuto powered script plays media (video or audio) files using the HTML5
tag embedded in an HTML file (specified in the GetPlayerHTMLFileName() method)
and asserts proper event occurrence relating to actions. The parameters
needed to run this test are passed in the form of environment variables
(such as the number of runs). Media_perf_runner.py is used for generating
these variables (PyAuto does not support direct parameters).
"""

import os

import pyauto_media
from media_event_test_base import MediaEventTestBase
from media_test_env_names import MediaTestEnvNames


class MediaEventSimpleActionTest(MediaEventTestBase):
  """Tests for simple media events with actions."""

  def PreAllRunsProcess(self):
    """A method to execute before each run.

    This function defines simple series of actions (e.g., pause, play, seek).
    """
    # This environment variable is used in
    # MediaEventTestBase.PreAllRunsProcess(), which sets the query string of
    # data/media/html/media_event.html. It performs these actions
    # based on query strings.
    os.environ[MediaTestEnvNames.TEST_SCENARIO_ENV_NAME] = (
        '300|pause|0|400|play|0|500|seek|0')
    MediaEventTestBase.PreAllRunsProcess(self)

  def testHTML5MediaTag(self):
    """Test the HTML5 media tag."""
    MediaEventTestBase.ExecuteTest(self)


if __name__ == '__main__':
  pyauto_media.Main()
