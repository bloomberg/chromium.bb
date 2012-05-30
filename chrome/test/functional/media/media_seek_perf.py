#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Seek performance testing for <video>.

Calculates the short and long seek times for different video formats on
different network constraints.
"""

import logging
import os

import pyauto_media
import pyauto_utils

import cns_test_base
import worker_thread

# Number of threads to use during testing.
_TEST_THREADS = 3

# HTML test path; relative to src/chrome/test/data.
_TEST_HTML_PATH = os.path.join('media', 'html', 'media_seek.html')

# The media files used for testing.
# Path under CNS root folder (pyauto_private/media).
_TEST_VIDEOS = [os.path.join('dartmoor', name) for name in
                ['dartmoor2.ogg', 'dartmoor2.m4a', 'dartmoor2.mp3',
                 'dartmoor2.wav']]
_TEST_VIDEOS.extend(os.path.join('crowd', name) for name in
                    ['crowd1080.webm', 'crowd1080.ogv', 'crowd1080.mp4',
                     'crowd360.webm', 'crowd360.ogv', 'crowd360.mp4'])

# Constraints to run tests on.
_TESTS_TO_RUN = [
    cns_test_base.Cable,
    cns_test_base.Wifi,
    cns_test_base.NoConstraints]


class SeekWorkerThread(worker_thread.WorkerThread):
  """Worker thread.  Runs a test for each task in the queue."""

  def RunTask(self, unique_url, task):
    """Runs the specific task on the url given.

    It is assumed that a tab with the unique_url is already loaded.
    Args:
      unique_url: A unique identifier of the test page.
      task: A (series_name, settings, file_name) tuple to run the test on.
    """
    series_name, settings, file_name = task

    video_url = cns_test_base.GetFileURL(
        file_name, bandwidth=settings[0], latency=settings[1],
        loss=settings[2])

    # Start the test!
    self.CallJavascriptFunc('startTest', [video_url], unique_url)

    logging.debug('Running perf test for %s.', video_url)
    # Time out is dependent on (seeking time * iterations).  For 3 iterations
    # per seek we get total of 18 seeks per test.  We expect buffered and
    # cached seeks to be fast.  Through experimentation an average of 10 secs
    # per seek was found to be adequate.
    if not self.WaitUntil(self.GetDOMValue, args=['endTest', unique_url],
                          retry_sleep=5, timeout=300, debug=False):
      error_msg = 'Seek tests timed out.'
    else:
      error_msg = self.GetDOMValue('errorMsg', unique_url)

    cached_states = self.GetDOMValue(
        "Object.keys(CachedState).join(',')", unique_url).split(',')
    seek_test_cases = self.GetDOMValue(
        "Object.keys(SeekTestCase).join(',')", unique_url).split(',')

    graph_name = series_name + '_' + os.path.basename(file_name)
    for state in cached_states:
      for seek_case in seek_test_cases:
        values = self.GetDOMValue(
            "seekRecords[CachedState.%s][SeekTestCase.%s].join(',')" %
            (state, seek_case), unique_url)
        if values:
          results = [float(value) for value in values.split(',')]
        else:
          results = []
        pyauto_utils.PrintPerfResult('seek', '%s_%s_%s' %
                                     (state, seek_case, graph_name),
                                     results, 'sec')

    if error_msg:
      logging.error('Error while running %s: %s.', graph_name, error_msg)
      return False
    else:
      return True


class MediaSeekPerfTest(cns_test_base.CNSTestBase):
  """PyAuto test container.  See file doc string for more information."""

  def __init__(self, *args, **kwargs):
    """Initialize the CNSTestBase with socket_timeout = 60 secs."""
    cns_test_base.CNSTestBase.__init__(self, socket_timeout='60',
                                       *args, **kwargs)

  def testMediaSeekPerformance(self):
    """Launches HTML test which plays each video and records seek stats."""
    tasks = cns_test_base.CreateCNSPerfTasks(_TESTS_TO_RUN, _TEST_VIDEOS)
    if worker_thread.RunWorkerThreads(self, SeekWorkerThread, tasks,
                                      _TEST_THREADS, _TEST_HTML_PATH):
      self.fail('Some tests failed to run as expected.')


if __name__ == '__main__':
  pyauto_media.Main()
