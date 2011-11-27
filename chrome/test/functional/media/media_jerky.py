#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple test for HTML5 media tag to measure playback time.

This PyAuto powered script plays media (video or audio) files using the HTML5
tag embedded in an HTML file (specified in the GetPlayerHTMLFileName() method)
for a test that uses a jerky tool that is being developed. The tool will
measure display Frame-per-second (FPS) (instead of decode FPS, which is
measured in media_fps.py). The tool requires predefined pattern to show before
video plays to determine the area to be measured (it is done in
data/media/html/media_jerky.html).

The parameters needed to run this test are passed in the form of environment
variables (such as the number of runs). Media_perf_runner.py is used for
generating these variables (PyAuto does not support direct parameters).
"""
import logging
import os
import subprocess
import time

from media_test_base import MediaTestBase
from media_test_env_names import MediaTestEnvNames
import pyauto_media


class MediaJerkyTest(MediaTestBase):
  """Test class to use the jerky tool."""
  # A name list comes from the jerky tool.
  # JERKY_NAME_LIST, JERKY_UNIT_LIST and JERKY_TRACE_LIST correspond to each
  # other.
  JERKY_NAME_LIST = ['jerkiness', 'frame drawing rate',
                     'torn frames', 'skipped frames']
  JERKY_UNIT_LIST = ['msec', 'count', 'count', 'count']
  # A trace list is used for legends in graph. One letter is
  # preferable for trace and can be any one character letter. Here
  # I use the first letter of each unit except for 'sec'.
  JERKY_TRACE_LIST = ['t', 'c', 'c', 'c']

  jerky_tool_output_filename_base = ''

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

  def PreEachRunProcess(self, run_counter):
    """Starts the jerky tool.  Executes before each test run.

    Args:
      run_counter: counter for each run.
    """
    MediaTestBase.PreEachRunProcess(self, run_counter)

    jerky_tool_binary_loc = os.getenv(
        MediaTestEnvNames.JERKY_TOOL_BINARY_LOCATION_ENV_NAME, '')
    jerky_tool_output_dir = os.getenv(
        MediaTestEnvNames.JERKY_TOOL_OUTPUT_DIR_ENV_NAME, '')
    filename = self.media_filename_nickname + '_' + str(run_counter)
    self.jerky_tool_output_filename_base = os.path.join(
        jerky_tool_output_dir, self.media_filename_nickname)
    jerky_tool_output_filename = (
        os.path.join(jerky_tool_output_dir, filename))
    jerky_tool_baseline_dir = os.getenv(
        MediaTestEnvNames.JERKY_TOOL_BASELINE_DIR_ENV_NAME, '')
    jerky_tool_baseline_filename = (
        os.path.join(jerky_tool_baseline_dir, filename))
    if jerky_tool_binary_loc:
      # TODO(imasaki@): change the following when jerky tool is implemented.
      cmd = '%s -o %s -r %s' % (jerky_tool_binary_loc,
                                jerky_tool_output_filename,
                                jerky_tool_baseline_filename)
      proc = subprocess.Popen(cmd, shell=True)
      proc.communicate()

  def PostAllRunsProcess(self):
    """Collects the jerky tool results.  Executes after all test runs.

    Jerky tool writes the output for each run in the following format
    v1('jerkiness'), v2('frame drawing rate'), v3('torn frames'),
    v4('skipped frames') to the file specified by the commandline
    argument.
    """
    # Storing inserted data that is necessary for perf graphs.
    data_for_perf_graphs = []
    # Initialize |data_for_perf_graphs|, which is a 2-D array.
    for i in xrange(len(self.JERKY_NAME_LIST)):
      data_for_perf_graphs.append([])
    for i in xrange(self.number_of_runs):
      jerky_output_filename = (
          self.jerky_tool_output_filename_base + '_' + str(i))
      jerky_output_file = open(jerky_output_filename, 'r')
      # Jerky output data is delimited by commas.
      jerky_output_data = jerky_output_file.read().split(',')
      if len(jerky_output_data) != len(self.JERKY_NAME_LIST):
        logging.warning(
            ('Jerky output file (%s) only has %d data elements. '
             'It should have %d elements') % (jerky_output_filename,
                                              len(jerky_output_data),
                                              len(self.JERKY_NAME_LIST)))
        continue
      for j in xrange(len(jerky_output_data)):
        data_for_perf_graphs[j].append(jerky_output_data[j])
      jerky_output_file.close()
    for i in xrange(len(data_for_perf_graphs)):
      print 'RESULT %s: %s= %s %s' % (self.JERKY_NAME_LIST[i],
                                      self.JERKY_TRACE_LIST[i],
                                      data_for_perf_graphs[i],
                                      self.JERKY_UNIT_LIST[i])


if __name__ == '__main__':
  pyauto_media.Main()
