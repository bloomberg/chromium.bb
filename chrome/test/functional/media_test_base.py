#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A framework to run PyAuto HTML media tests.

This PyAuto powered script plays media (video or audio) files (using HTML5 tag
embedded in player.html file). The parameters needed to run this test are
passed in the form of environment variables (such as the number of runs).
media_test_runner.py is used for generating these variables
(PyAuto does not support direct parameters).
"""
import os
import time

import pyauto_functional  # Must be imported before pyauto.
import pyauto

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
  # Since PyAuto does not support commandline argument, we have to rely on
  # environment variables. The followings are the names of the environment
  # variables that are used in the tests. PLAYER_HTML_URL_NICKNAME is used to
  # display the result output in compact form (e.g., "local", "remote").
  PLAYER_HTML_URL_NICKNAME_ENV_NAME = 'PLAYER_HTML_URL_NICKNAME'
  # Default base url nick name used to display the result in case it is not
  # specified by the environment variable.
  DEFAULT_PLAYER_HTML_URL_NICKNAME = 'local'
  PLAYER_HTML_URL_ENV_NAME = 'PLAYER_HTML_URL'
  # Use this when you want to add extra information in result output.
  EXTRA_NICKNAME_ENV_NAME = 'EXTRA_NICKNAME'
  # Use this when you do not want to report the first result output.
  # First result includes time to start up the browser.
  REMOVE_FIRST_RESULT_ENV_NAME = 'REMOVE_FIRST_RESULT'
  # Add t=Data() parameter in query string to disable media cache.
  ADD_T_PARAMETER_ENV_NAME = 'ADD_T_PARAMETER'
  # Print out only playback time information (neither CPU nor memory).
  PRINT_ONLY_TIME_ENV_NAME = 'PRINT_ONLY_TIME'
  # Define the number of tries.
  N_RUNS_ENV_NAME = 'N_RUNS'
  # Define tag name in HTML (either video or audio).
  MEDIA_TAG_ENV_NAME = 'HTML_TAG'
  # Define media file name.
  MEDIA_FILENAME_ENV_NAME = 'MEDIA_FILENAME'
  # Define media file nickname that is used for display.
  MEDIA_FILENAME_NICKNAME_ENV_NAME = 'MEDIA_FILENAME_NICKNAME'
  # Default values used for default case.
  DEFAULT_MEDIA_TAG_NAME = 'video'
  DEFAULT_MEDIA_FILENAME = 'bear_silent.ogv'
  DEFAULT_NUMBER_OF_RUNS = 3
  TIMEOUT = 10000
  # Instance variables that used across methods.
  number_of_runs = 0
  url = ''
  parameter_str = ''
  times = []
  media_filename = ''
  media_filename_nickname = ''

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
    player_html_url = os.getenv(self.PLAYER_HTML_URL_ENV_NAME, 'DEFAULT')
    player_html_url_nickname = os.getenv(
        self.PLAYER_HTML_URL_NICKNAME_ENV_NAME,
        self.DEFAULT_PLAYER_HTML_URL_NICKNAME)
    extra_nickname = os.getenv(self.EXTRA_NICKNAME_ENV_NAME, '')
    # This parameter tricks the media cache into thinking
    # it's a new file every time.
    # However, it looks like does not make much difference in
    # performance.
    add_t_parameter = os.getenv(self.ADD_T_PARAMETER_ENV_NAME) in ('Y', 'y')
    # Print only playback time data.
    print_only_time = os.getenv(self.PRINT_ONLY_TIME_ENV_NAME) in ('Y', 'y')
    tag = os.getenv(self.MEDIA_TAG_ENV_NAME, self.DEFAULT_MEDIA_TAG_NAME)
    if add_t_parameter:
      # This can be any string and setting this disables the media cache.
      t_para_query_str = '=t_para'
    else:
      t_para_query_str = ''
    query_str = tag + '=' + media_filename + t_para_query_str
    if player_html_url_nickname == self.DEFAULT_PLAYER_HTML_URL_NICKNAME:
      # Default is local file under DataDir().
      file_url = self.GetFileURLForDataPath(
          os.path.join('media', 'player_for_perf.html'))
      url = file_url + '?' + query_str
    else:
      url = player_html_url + '?' + query_str
    parameter_str = 'tpara_%s-%s-%s' % (str(add_t_parameter),
                                        player_html_url_nickname,
                                        extra_nickname)
    return url, parameter_str

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
    for run_counter in range(self.number_of_runs):
      self.PreEachRunProcess(run_counter)
      self.NavigateToURL(self.url)
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
    self.media_filename = os.getenv(self.MEDIA_FILENAME_ENV_NAME,
                               self.DEFAULT_MEDIA_FILENAME)
    self.remove_first_result = (
        os.getenv(self.REMOVE_FIRST_RESULT_ENV_NAME) in ('Y', 'y'))
    self.number_of_runs = int(os.getenv(self.N_RUNS_ENV_NAME,
                                        self.DEFAULT_NUMBER_OF_RUNS))
    self.url, self.parameter_str = self._GetMediaURLAndParameterString(
       self.media_filename)
    self.times = []

  def PostAllRunsProcess(self):
    """A method to execute after all runs.

    The default behavior is to print out the playback time data.
    """

    self.media_filename_nickname = os.getenv(
        self.MEDIA_FILENAME_NICKNAME_ENV_NAME, self.media_filename)
    # Print out playback time for each run.
    print UIPerfTestUtils.PrintResultsImpl(
        measurement='playback-' + self.parameter_str, modifier='',
        trace=self.media_filename_nickname, values=self.times[1:],
        units='sec')

  def PreEachRunProcess(self, run_counter):
    """A method to execute before each run.

    The default behavior is to record start time.

    Args:
      run_counter: counter for each run.
    """
    self.start = time.time()

  def PostEachRunProcess(self, run_counter):
    """A method to execute after each run.

    The default behavior is to calculate and store playback time for each run.

    Args:
      run_counter: counter for each run.
    """
    if not self.remove_first_result or run_counter > 0:
      self.times.append(time.time() - self.start)


if __name__ == '__main__':
  pyauto_functional.Main()
