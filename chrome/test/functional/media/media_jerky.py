#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Jerkiness performance test for video playback.

Uses jerky tool, (http://go/jerky), to record a jerkiness metric for videos
sensitive to jerkiness.

Jerkiness is defined as a percentage of the average on screen frame time by the
formula below.  Where smoothed_frame_time[i] represents a frame's on screen time
plus amortized measurement gap error (time taken to capture each frame).

sqrt(average((avg_frame_time - smoothed_frame_time[i])^2, i=m..n))
------------------------------------------------------------------
                          avg_frame_time

Currently, only the Linux binaries are checked in for this test since we only
have a Linux performance bot.  The current binary is a custom build with some
fixes from veganjerky (http://go/veganjerky) for timing, waiting, and stdout
flushing.

TODO(dalecurtis): Move Jerky tool sources into the Chromium tree.

TODO(dalecurtis): Jerky tool uses a questionable method for determining torn
frames, determine if it is actually worth recording.
"""

import glob
import logging
import os
import re
import subprocess
import tempfile

import pyauto_media
import pyauto
import pyauto_utils

# HTML test path; relative to src/chrome/test/data.
_TEST_HTML_PATH = os.path.join('media', 'html', 'media_jerky.html')

# Path under data path for test files.
_TEST_MEDIA_PATH = os.path.join('pyauto_private', 'media', 'birds')

# Path to Jerky tool executable.
_JERKY_PATH = os.path.join('pyauto_private', 'media', 'tools', 'jerky')

# Regular expression for extracting jerkiness percentage.  Sample line:
#   using 1:9 title 'test.log (35.36% jerky, 0 teared frames)' lw 2 with lines
_JERKY_LOG_REGEX = re.compile(
    r'\((\d{0,3}\.?\d{0,2})% jerky, (\d+) teared frames\)')

# Regular expression for extracting computed fps.  Sample line:
# INFO: 33797 us per frame => 29.6 fps.
_JERKY_LOG_FPS_REGEX = re.compile(r' => (\d+\.\d+) fps')

# Minimum and maximum number of iterations for each test.  Due to timing issues
# the jerky tool may not always calculate the fps correctly.  When invalid
# results are detected, the test is rerun up to the maxium # of times set below.
_JERKY_ITERATIONS_MIN = 3
_JERKY_ITERATIONS_MAX = 10

# The media files used for testing.  Each entry represents a tuple of (filename,
# width, height, fps).  The width and height are used to create a calibration
# pattern for jerky tool.  The fps is used to ensure Jerky tool computed a valid
# result.
_TEST_VIDEOS = [('birds540.webm', 960, 540, 29.9)]


def GetTempFilename():
  """Returns an absolute path to an empty temp file."""
  f, path = tempfile.mkstemp(prefix='jerky_tmp')
  os.close(f)
  return path


class MediaJerkyPerfTest(pyauto.PyUITest):
  """PyAuto test container.  See file doc string for more information."""

  def StartJerkyCapture(self):
    """Starts jerky tool in capture mode and waits until its ready to capture.

    Returns:
      A tuple of the jerky process and an absolute path to the capture log.
    """
    jerky_log = GetTempFilename()
    logging.debug('Logging data to %s', jerky_log)
    process = subprocess.Popen(
        [os.path.join(self.DataDir(), _JERKY_PATH),
         'capture', '--log', jerky_log],
        stdout=subprocess.PIPE)

    # Set the jerky tool process to soft-realtime w/ round-robin scheduling.
    subprocess.check_call(['sudo', 'chrt', '-r', '-p', str(process.pid)])

    # Wait for server to start up.
    line = True
    while line:
      line = process.stdout.readline()
      if 'Waiting for calibration pattern to disappear' in line:
        return process, jerky_log
    self.fail('Failed to launch Jerky tool.')

  def AnalyzeJerkyCapture(self, jerky_log):
    """Run jerky analyze on the specified log and return various metrics.

    Once analyze has completed, the jerky_log and associated outputs will be
    removed.

    Args:
      jerky_log: Absolute path to the capture log.

    Returns:
      Tuple of fps, jerkiness, and torn frames.
    """
    results_log_base = GetTempFilename()
    process = subprocess.Popen(
        [os.path.join(self.DataDir(), _JERKY_PATH),
         'analyze', '--ref', jerky_log, '--out', results_log_base],
        stdout=subprocess.PIPE)

    # Wait for process to end w/o risking deadlock.
    stdout = process.communicate()[0]
    self.assertEquals(process.returncode, 0)

    # Scrape out the calculated FPS.
    fps_match = None
    for line in stdout.splitlines():
      fps_match = _JERKY_LOG_FPS_REGEX.search(line)
      if fps_match:
        break

    # Open *.error.gnuplot and scrape out jerkiness.
    jerky_match = None
    with open('%s.error.gnuplot' % results_log_base) as results:
      for line in results:
        jerky_match = _JERKY_LOG_REGEX.search(line)
        if jerky_match:
          break

    # Cleanup all the temp and results files jerky spits out.
    for log in glob.glob('%s*' % results_log_base) + [jerky_log]:
      os.unlink(log)

    if fps_match and jerky_match:
      return (float(fps_match.group(1)), float(jerky_match.group(1)),
              int(jerky_match.group(2)))
    return None, None, None

  def testMediaJerkyPerformance(self):
    """Launches Jerky tool and records jerkiness for HTML5 videos.

    For each video, the test starts up jerky tool then plays until the Jerky
    tool collects enough information.  Next the capture log is analyzed using
    Jerky's analyze command.  If the computed fps matches the expected fps the
    jerkiness metric is recorded.

    The test will run up to _JERKY_ITERATIONS_MAX times in an attempt to get at
    least _JERKY_ITERATIONS_MIN valid values.  The results are recorded under
    the 'jerkiness' variable for graphing on the bots.
    """
    self.NavigateToURL(self.GetFileURLForDataPath(_TEST_HTML_PATH))

    # Xvfb on the bots is restricted to 1024x768 at present.  Ensure we're using
    # all of the real estate we can.  Jerky tool needs a clear picture of every
    # frame, so we can't clip the video in any way.
    self.SetWindowDimensions(0, 0, 1024, 768)

    for name, width, height, expected_fps in _TEST_VIDEOS:
      jerkiness = []
      torn_frames = []
      file_url = self.GetFileURLForDataPath(
          os.path.join(_TEST_MEDIA_PATH, name))

      # Initialize the calibration area for Jerky tool.
      self.assertTrue(self.ExecuteJavascript(
          'initializeTest(%d, %d);' % (width, height)))

      runs_left = _JERKY_ITERATIONS_MIN
      runs_total = 0
      while runs_left > 0 and runs_total < _JERKY_ITERATIONS_MAX:
        runs_total += 1
        logging.info('Running Jerky perf test #%d for %s.', runs_total, name)

        # Startup Jerky tool in capture mode.
        jerky_process, jerky_log = self.StartJerkyCapture()

        # Start playback of the test video.
        self.assertTrue(self.ExecuteJavascript("startTest('%s');" % file_url))

        # Wait for jerky tool to finish if it hasn't already.
        self.assertTrue(jerky_process.wait() == 0)

        # Stop playback of the test video so the next run can cleanly find the
        # calibration zone.
        self.assertTrue(self.ExecuteJavascript('stopTest();'))

        # Analyze the results.
        jerky_fps, jerky_percent, jerky_torn_frames = self.AnalyzeJerkyCapture(
            jerky_log)
        if (jerky_fps is None or jerky_percent is None or
            jerky_torn_frames is None):
          logging.error('No metrics recorded for this run.')
          continue

        # Ensure the results for this run are valid.
        if jerky_fps != expected_fps:
          logging.error(
              'Invalid fps detected (actual: %f, expected: %f, jerkiness: %f). '
              'Discarding results for this run.', jerky_fps, expected_fps,
              jerky_percent)
          continue

        jerkiness.append(jerky_percent)
        torn_frames.append(jerky_torn_frames)
        runs_left -= 1

      pyauto_utils.PrintPerfResult('jerkiness', name, jerkiness, '%')


if __name__ == '__main__':
  pyauto_media.Main()
