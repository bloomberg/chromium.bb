#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A framework to run PyAuto HTML media tests.

This PyAuto powered script plays media (video or audio) files (using the HTML5
tag embedded in an HTML file). The parameters needed to run this test are
passed in the form of environment variables (such as the number of runs).
media_test_runner.py is used for generating these variables
(PyAuto does not support direct parameters).
"""

import csv
import os
import time

import pyauto_media
import pyauto

from media_test_env_names import MediaTestEnvNames
from ui_perf_test_utils import UIPerfTestUtils


class MediaTestBase(pyauto.PyUITest):
  """A base class for media related PyAuto tests.

  This class is meant to a base class for all media related Pyauto test and
  provides useful functionality to run the test in conjunction with
  player.html file, which contains basic html and JavaScipt for media test.
  The main test method (ExecuteTest()) contains execution loops to get average
  measured data over several runs with the same condition. This class also
  contains several basic pre/post-processing methods that should be overridden.
  """

  # Default values used for default case.
  DEFAULT_MEDIA_TAG_NAME = 'video'
  DEFAULT_MEDIA_FILENAME = 'bear_silent.ogv'
  DEFAULT_PLAYER_HTML_URL_NICKNAME = 'local'
  DEFAULT_NUMBER_OF_RUNS = 3
  TIMEOUT = 10000
  # Instance variables that used across methods.
  number_of_runs = 0
  url = ''
  parameter_str = ''
  times = []
  media_filename = ''
  media_filename_nickname = ''
  _test_scenarios = []
  # Used for tracing performance results on PerfBot: can be 't' (tests with
  # normal build) or 't_ref' (tests with reference build).
  current_trace_type = 't'

  def _GetMediaURLAndParameterString(self, media_filename):
    """Get media url and parameter string.

    If media url is specified in environment variable, then it is used.
    Otherwise, local media data directory is used for the url.
    Parameter string is calculated based on the environment variables.

    Args:
      media_filename: the file name for the media (video/audio) with extension.

    Returns:
      a tuple of media_url (with proper query string) and a parameter string,
        which is used for performance result display.
    """
    # Read environment variables.
    player_html_url = os.getenv(MediaTestEnvNames.PLAYER_HTML_URL_ENV_NAME,
                                'DEFAULT')
    player_html_url_nickname = os.getenv(
        MediaTestEnvNames.PLAYER_HTML_URL_NICKNAME_ENV_NAME,
        self.DEFAULT_PLAYER_HTML_URL_NICKNAME)
    extra_nickname = os.getenv(MediaTestEnvNames.EXTRA_NICKNAME_ENV_NAME, '')

    tag = os.getenv(MediaTestEnvNames.MEDIA_TAG_ENV_NAME,
                    self.DEFAULT_MEDIA_TAG_NAME)
    query_dictionary = {'tag': tag, 'media': media_filename}
    # This parameter tricks the media cache into thinking
    # it's a new file every time.
    # However, it looks like does not make much difference in
    # performance.
    if os.getenv(MediaTestEnvNames.ADD_T_PARAMETER_ENV_NAME):
      query_dictionary['t'] = 'dummy'
    query_str = '&'.join(
        [k + '=' + str(v) for (k, v) in query_dictionary.items()])
    if player_html_url_nickname == self.DEFAULT_PLAYER_HTML_URL_NICKNAME:
      # Default is local file under DataDir().
      file_url = self.GetFileURLForDataPath(
          os.path.join('media', 'html', self.GetPlayerHTMLFileName()))
      url = file_url + '?' + query_str
    else:
      url = player_html_url + '?' + query_str
    parameter_str = '%s_%s_%s' % (
        extra_nickname, player_html_url_nickname,
        os.getenv(MediaTestEnvNames.MEDIA_FILENAME_NICKNAME_ENV_NAME))
    return url, parameter_str

  def ReadTestScenarioFiles(self, test_scenario_filename):
    """Read a test scenario CSV file with actions such as 'play'.

    In the CSV file, each row is a test scenario which consists of one
    or more (time, action, action_argument) triples (time and action_argument
    are in milliseconds). For example, the following CSV file contains 3 test
    scenarios to be tested.
    500,  pause, 0
    1000, pause, 0, 2000, play,       0
    1000, seek,  0, 2000, ratechange, 2
    """
    test_scenarios = []
    rows = csv.reader(open(test_scenario_filename))
    for row in rows:
      test_scenarios.append('|'.join(row))
    return test_scenarios

  def ExecuteTest(self):
    """Test HTML5 Media Tag."""

    def _VideoEnded():
      """Determine if the video ended.

      When the video has finished playing, its title is updated by player.html.

      Returns:
        True if the video has ended.
      """
      return self.GetDOMValue('document.title').strip() == 'END'

    self.PreAllRunsProcess()
    test_scenario_filename = os.getenv(
        MediaTestEnvNames.TEST_SCENARIO_FILE_ENV_NAME, '')
    test_scenario = os.getenv(
        MediaTestEnvNames.TEST_SCENARIO_ENV_NAME, '')
    if test_scenario:
      # Run test with the same action several times.
      self._test_scenarios = [test_scenario] * self.number_of_runs
    if test_scenario_filename:
      self._test_scenarios = self.ReadTestScenarioFiles(test_scenario_filename)
      # One run per test scenario.
      self.number_of_runs = len(self._test_scenarios)
    for run_counter in range(self.number_of_runs):
      self.PreEachRunProcess(run_counter)
      url = self.url
      if self._test_scenarios:
        url += '&actions=' + self.test_scenarios[run_counter]
      self.NavigateToURL(url)
      self.WaitUntil(lambda: _VideoEnded(),
                     self.TIMEOUT)
      self.PostEachRunProcess(run_counter)

    self.PostAllRunsProcess()

  # A list of methods that should be overridden in the subclass.
  # It is a good practice to call these methods even if these are
  # overridden.

  def PreAllRunsProcess(self):
    """A method to be executed before all runs.

    The default behavior is to read parameters for the tests and initialize
    variables.
    """
    self.media_filename = os.getenv(MediaTestEnvNames.MEDIA_FILENAME_ENV_NAME,
                                    self.DEFAULT_MEDIA_FILENAME)
    self.remove_first_result = os.getenv(
        MediaTestEnvNames.REMOVE_FIRST_RESULT_ENV_NAME)
    self.number_of_runs = int(os.getenv(MediaTestEnvNames.N_RUNS_ENV_NAME,
                                        self.DEFAULT_NUMBER_OF_RUNS))
    self.url, self.parameter_str = self._GetMediaURLAndParameterString(
        self.media_filename)
    self.times = []

  def PostAllRunsProcess(self):
    """A method to execute after all runs.

    The default behavior is to do nothing
    """
    pass

  def PreEachRunProcess(self, run_counter):
    """A method to execute before each run.

    The default behavior is to record start time.

    Args:
      run_counter: counter for each run.
    """
    self.start = time.time()
    if os.getenv(MediaTestEnvNames.REFERENCE_BUILD_ENV_NAME):
      self.current_trace_type = 't_ref'

  def PostEachRunProcess(self, run_counter):
    """A method to execute after each run.

    The default behavior is to do nothing.

    Args:
      run_counter: counter for each run.
    """
    pass

  def GetPlayerHTMLFileName(self):
    """A method to get the player HTML file name."""
    return 'media_playback.html'


if __name__ == '__main__':
  pyauto_media.Main()
