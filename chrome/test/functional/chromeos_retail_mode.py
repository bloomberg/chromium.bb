# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # Must come before pyauto (and thus, policy_base).
import policy_base


class ChromeosRetailMode(policy_base.PolicyTestBase):
  """Tests for retail mode."""

  # The inherited setUp() method fakes enterprise enrollment. Setting the mode
  # to 'kiosk' causes enrollment into retail mode instead of the default
  # enterprise mode.
  mode = 'kiosk'
  machine_id = 'KIOSK'

  def ShouldOOBESkipToLogin(self):
    # There's no OOBE to skip.
    return False

  def _CheckOnRetailModeLoginScreen(self):
    """Checks that the retail mode login screen is visible."""
    return self.ExecuteJavascriptInOOBEWebUI(
        """window.domAutomationController.send(
               !!document.getElementById('demo-login-text'));
        """)

  def Login(self):
    """Login to retail mode by simulating a mouse click."""
    self.ExecuteJavascriptInOOBEWebUI(
        """var event = document.createEvent("MouseEvent");
           event.initMouseEvent('click', true, true, window, 0, 0, 0, 0, 0,
               false, false, false, false, 0, null);
           window.domAutomationController.send(
              document.getElementById('page').dispatchEvent(event));
        """)

  def testLogin(self):
    """Tests retail mode login."""
    self.assertTrue(self._CheckOnRetailModeLoginScreen(),
                    msg='Expected to be on the retail mode login screen.')

    self.Login()
    self.assertTrue(self.GetLoginInfo()['is_logged_in'],
                    msg='Expected to be logged in.')
    self.Logout()


if __name__ == '__main__':
  pyauto_functional.Main()
