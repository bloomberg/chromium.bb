#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""CPU, Memory, and FPS performance test for <video>.

Calculates decoded fps, dropped fps, CPU, and memory statistics while playing
HTML5 media element. The test compares results of playing a media file on
different video resolutions.
"""

import logging
import os
import psutil

import pyauto_media
import pyauto
import pyauto_utils

# HTML test path; relative to src/chrome/test/data.
_TEST_HTML_PATH = os.path.join('media', 'html', 'media_stat_perf.html')

# Path under data path for test files.
_TEST_MEDIA_PATH_CROWD = os.path.join('pyauto_private', 'media', 'crowd')

# Path under data path for test files.
_TEST_MEDIA_PATH_TULIP = os.path.join('media', 'avperf', 'tulip')

# The media files used for testing.
_TEST_VIDEOS = [os.path.join(_TEST_MEDIA_PATH_CROWD, name) for name in [
    'crowd2160.webm', 'crowd1080.webm', 'crowd720.webm', 'crowd480.webm',
    'crowd360.webm']]

_TEST_VIDEOS.extend([os.path.join(_TEST_MEDIA_PATH_TULIP, name) for name in [
    'tulip2.webm', 'tulip2.wav', 'tulip2.ogv', 'tulip2.ogg', 'tulip2.mp4',
    'tulip2.mp3', 'tulip2.m4a']])


class MediaStatsPerfTest(pyauto.PyUITest):
  """PyAuto test container.  See file doc string for more information."""

  def _GetChromeRendererProcess(self):
    """Returns the Chrome renderer process."""
    renderer_id = self.GetBrowserInfo()['windows'][0]['tabs'][1]['renderer_pid']
    if not renderer_id:
      self.fail('Can not find the tab renderer process.')
    return psutil.Process(renderer_id)

  def testMediaPerformance(self):
    """Launches HTML test which plays each video and records statistics."""
    for file_name in _TEST_VIDEOS:
      # Append a tab and delete it at the end of the test to free its memory.
      self.AppendTab(pyauto.GURL(self.GetFileURLForDataPath(_TEST_HTML_PATH)))

      file_url = self.GetFileURLForDataPath(file_name)
      logging.debug('Running perf test for %s.', file_url)

      renderer_process = self._GetChromeRendererProcess()
      # Call to set a starting time to record CPU usage by the renderer.
      renderer_process.get_cpu_percent()

      self.assertTrue(
          self.CallJavascriptFunc('startTest', [file_url], tab_index=1))

      cpu_usage = renderer_process.get_cpu_percent()
      mem_usage_mb = renderer_process.get_memory_info()[0] / 1024
      file_name = os.path.basename(file_name)
      pyauto_utils.PrintPerfResult('cpu', file_name, cpu_usage, '%')
      pyauto_utils.PrintPerfResult('memory', file_name, mem_usage_mb, 'KB')

      decoded_fps = [
          float(value) for value in
          self.GetDOMValue("decodedFPS.join(',')", tab_index=1).split(',')]
      dropped_frames = self.GetDOMValue('droppedFrames', tab_index=1)
      dropped_fps = [
          float(value) for value in
          self.GetDOMValue("droppedFPS.join(',')", tab_index=1).split(',')]

      pyauto_utils.PrintPerfResult('fps', file_name, decoded_fps, 'fps')
      pyauto_utils.PrintPerfResult('dropped_fps', file_name, dropped_fps, 'fps')
      pyauto_utils.PrintPerfResult('dropped_frames', file_name, dropped_frames,
                                   'frames')

      self.CloseTab(tab_index=1)


if __name__ == '__main__':
  pyauto_media.Main()
