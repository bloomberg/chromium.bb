#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import pyauto_functional  # Must be imported before pyauto
import pyauto

class ChromeosTime(pyauto.PyUITest):
  """Tests for the ChromeOS status area clock and timezone settings."""

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self._initial_timezone = self.GetTimeInfo()['timezone']

  def tearDown(self):
    self.SetTimezone(self._initial_timezone)
    pyauto.PyUITest.tearDown(self)

  def testTimeInfo(self):
    """Print the the display time, date, and timezone."""
    logging.debug(self.GetTimeInfo())

  def testSetTimezone(self):
    """Sanity test to make sure setting the timezone works."""
    self.SetTimezone('America/Los_Angeles')
    pacific_time = self.GetTimeInfo()['display_time']
    self.SetTimezone('America/New_York')
    eastern_time = self.GetTimeInfo()['display_time']

    self.assertNotEqual(pacific_time, eastern_time,
                        'Time zone changed but display time did not.')


if __name__ == '__main__':
  pyauto_functional.Main()
