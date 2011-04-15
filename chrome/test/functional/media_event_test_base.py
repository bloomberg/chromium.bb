#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Event test base for the HTML5 media tag.

This class contains all common code needed for event testing. Most of the
methods should be overridden by the subclass.
"""

import pyauto_functional  # Must be imported before pyauto.
import pyauto

from media_test_base import MediaTestBase


class MediaEventTestBase(MediaTestBase):
  """Event test base for the HTML5 media tag."""
  # This is a list of events to test during media playback.
  EVENT_LIST = ['loadstart', 'ratechange', 'waiting',
                'ratechange', 'durationchange', 'loadedmetadata',
                'loadeddata', 'canplay', 'canplaythrough',
                'play', 'timeupdate', 'pause', 'ended']
  # These are event types that are not 1 at the end of video playback.
  # There are two types of events listed here:
  #   0: event occurrence is 0.
  #   None: event occurrence is more than 1.
  event_expected_values = {'ratechange': 0,
                           'pause': 0,
                           'timeupdate': None}

  def _GetEventLog(self):
    """Get the event log from the DOM tree that is produced by player.html.

    Returns:
      A dictionary mapping event names to the corresponding occurrence counts.
    """
    all_event_infos = {}
    for event_name in self.EVENT_LIST:
      loc = 'document.getElementById(\'%s\').innerHTML' % event_name
      value = self.GetDOMValue(loc).strip()
      all_event_infos[event_name] = int(value)
    return all_event_infos

  def AssertEvent(self, all_event_infos):
    """Assert event expected values.

    This could be overridden in the subclass.

    Args:
      all_event_infos: a dictionary that maps event names to the corresponding
        occurrence counts.
    """
    for event_name in self.EVENT_LIST:
      if event_name in self.event_expected_values:
        if self.event_expected_values[event_name] is None:
          self.assertTrue(
              all_event_infos[event_name] > 1,
              msg='the number of events should be more than 1 for %s' %
                  event_name)
        else:
          self.assertEqual(
              all_event_infos[event_name],
              self.event_expected_values[event_name],
              msg='the number of events is wrong for %s' % event_name)
      else:
        # Make sure the value is one.
        self.assertEqual(
            all_event_infos[event_name], 1,
            msg='the number of events should be 1 for %s' % event_name)

  def PostEachRunProcess(self, run_counter):
    """A method to execute after each run.

    Terminates the measuring thread and records the measurement in
    measure_thread.chrome_renderer_process_info.

    Args:
      run_counter: a counter for each run.
    """
    MediaTestBase.PostEachRunProcess(self, run_counter)
    all_event_infos = self._GetEventLog()
    self.AssertEvent(all_event_infos)

  def GetPlayerHTMLFileName(self):
    """A method to get the player HTML file name."""
    return 'media_event.html'


if __name__ == '__main__':
  pyauto_functional.Main()
