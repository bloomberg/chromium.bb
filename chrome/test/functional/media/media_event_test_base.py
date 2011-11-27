# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Event test base for the HTML5 media tag.

This class contains all common code needed for event testing. Most of the
methods should be overridden by the subclass.
"""

import copy

import pyauto_media
from media_test_base import MediaTestBase


class MediaEventTestBase(MediaTestBase):
  """Event test base for the HTML5 media tag."""
  # This is a list of events to test during media playback.
  EVENT_LIST = ['abort', 'canplay', 'canplaythrough', 'durationchange',
                'emptied', 'ended', 'error', 'load', 'loadeddata',
                'loadedmetadata', 'loadstart', 'pause', 'play', 'playing',
                'progress', 'ratechange', 'seeked', 'seeking', 'stalled',
                'suspend', 'timeupdate', 'volumechange', 'waiting',
                # Track related events.
                'cuechange', 'enter', 'exit', 'change']
  # These are event types that are not 1 at the end of video playback.
  # There are two types of events listed here:
  #   0: event occurrence is 0.
  #   None: event occurrence is more than 1.
  # When the event is listed in |EVENT_LIST| and not in
  # |event_expected_values|, it means that we do not assert the event
  # occurrence.
  # The following are default values that may be overridden.
  event_expected_values = {'play': 1,
                           'playing': 1,
                           'ratechange': 0,
                           'pause': 0,
                           'suspend': 0,
                           'load': 0,
                           'abort': 0,
                           'error': 0,
                           'emptied': 0,
                           'stalled': 0,
                           'seeking': 0,
                           'seeked': 0,
                           'volumechange': 0,
                           'timeupdate': None,
                           'cuechange': 0,
                           'enter': 0,
                           'exit': 0,
                           'change': 0}
  # A dictionary mapping event names to the corresponding related events.
  related_event_map = {'play': ['playing'], 'seek': ['seeked', 'seeking']}
  event_expected_values_for_each_run = {}

  def _GetEventLog(self):
    """Get the event log from the DOM tree that is produced by player.html.

    Returns:
      A dictionary mapping event names to the corresponding occurrence counts.
    """
    all_event_infos = {}
    for event_name in self.EVENT_LIST:
      loc = 'document.getElementById(\'%s\').innerHTML' % event_name
      all_event_infos[event_name] = self.GetDOMValue(loc).strip()
    return all_event_infos

  def AssertEvent(self, all_event_infos):
    """Assert event expected values.

    This could be overridden in the subclass.

    Args:
      all_event_infos: a dictionary that maps event names to the corresponding
        occurrence counts.
    """
    for event_name in self.EVENT_LIST:
      event_occurrence = len(all_event_infos[event_name].split())
      if event_name in self.event_expected_values_for_each_run:
        if self.event_expected_values_for_each_run[event_name] is None:
          self.assertTrue(
              event_occurrence > 1,
              msg='the number of events should be more than 1 for %s' %
                  event_name)
        else:
          self.assertEqual(
              event_occurrence,
              self.event_expected_values_for_each_run[event_name],
              msg='the number of events is wrong for %s' % event_name)
      else:
        # Make sure the value is one.
        self.assertEqual(
            event_occurrence, 1,
            msg='the number of events should be 1 for %s' % event_name)

  def _IncrementEventExpectedValue(self, action_name):
    """Increment event expected value for a given action.

    Args:
      action_name: the name of the action (e.g., 'play', 'pause', 'seek')
    """
    if (not action_name in self.event_expected_values_for_each_run or
        self.event_expected_values_for_each_run[action_name] is None):
      self.event_expected_values_for_each_run[action_name] = 0
    self.event_expected_values_for_each_run[action_name] += 1

  def PreEachRunProcess(self, run_counter):
    """A method to execute before each run.

    It refreshes the expected values for each run with original event
    expected values.

    Args:
      run_counter: a counter for each run.
    """
    self.event_expected_values_for_each_run = copy.copy(
        self.event_expected_values)

  def PostEachRunProcess(self, run_counter):
    """A method to execute after each run.

    Args:
      run_counter: a counter for each run.
    """
    MediaTestBase.PostEachRunProcess(self, run_counter)
    all_event_infos = self._GetEventLog()
    if self.whole_test_scenarios:
      test_scenario = self.whole_test_scenarios[self.run_counter]
      # Test scenario consists of list of triples 'time|action|target'
      # (e.g., '1000|pause|0' or '2000|seek|1000').
      test_scenario_elements = test_scenario.split('|')
      for i in xrange(len(test_scenario_elements) / 3):
        action_name = test_scenario_elements[i * 3 + 1]
        self._IncrementEventExpectedValue(action_name)
        if action_name in self.related_event_map:
          for related_event in self.related_event_map[action_name]:
            self._IncrementEventExpectedValue(related_event)
    self.AssertEvent(all_event_infos)

  def GetPlayerHTMLFileName(self):
    """A method to get the player HTML file name."""
    return 'media_event.html'
