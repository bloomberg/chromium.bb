#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import sys

import pyauto_functional  # Must be imported before pyauto
import pyauto

sys.path.append('/usr/local')  # To make autotest libs importable.
from autotest.cros import cros_ui
from autotest.cros import ownership


class ChromeosTime(pyauto.PyUITest):
  """Tests for the ChromeOS status area clock and timezone settings."""

  def setUp(self):
    cros_ui.fake_ownership()
    pyauto.PyUITest.setUp(self)
    self._initial_timezone = self.GetTimeInfo()['timezone']

  def tearDown(self):
    self.SetTimezone(self._initial_timezone)
    pyauto.PyUITest.tearDown(self)
    ownership.clear_ownership()

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

  def _IsTimezoneEditable(self):
    """Check if the timezone is editable.

    It will navigate to the system settings page and verify that the
    timezone settings drop down is not disabled.

    Returns:
      True, if timezone dropdown is enabled
      False, otherwise
    """
    self.NavigateToURL('chrome://settings-frame')
    ret = self.ExecuteJavascript("""
        var disabled = true;
        var timezone = document.getElementById('timezone-select');
        if (timezone)
          disabled = timezone.disabled;
        domAutomationController.send(disabled.toString());
    """)
    return ret == 'false'

  def testTimezoneIsEditable(self):
    """Test that the timezone is always editable."""
    # This test only makes sense if we are not running as the owner.
    self.assertFalse(self.GetLoginInfo()['is_owner'])
    editable = self._IsTimezoneEditable()
    self.assertTrue(editable, msg='Timezone is not editable when not owner.')


if __name__ == '__main__':
  pyauto_functional.Main()
