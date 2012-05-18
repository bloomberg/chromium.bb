# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import pyauto_functional  # Must come before pyauto (and thus, policy_base).
import policy_base


class ChromeosDevicePolicy(policy_base.PolicyTestBase):
  """Tests various ChromeOS device policies."""

  def _SetDevicePolicyAndOwner(self, policy):
    self.SetDevicePolicy(device_policy=policy, owner=self._owner)

  def Login(self, as_guest):
    self.assertFalse(self.GetLoginInfo()['is_logged_in'],
                     msg='Expected to be logged out.')
    if as_guest:
      self.LoginAsGuest()
    else:
      policy_base.PolicyTestBase.Login(self, self._owner, self._password)
    self.assertTrue(self.GetLoginInfo()['is_logged_in'],
                    msg='Expected to be logged in.')

  # TODO(bartfab): Remove this after crosbug.com/20709 is fixed.
  def TryToDisableLocalStateAutoClearing(self):
    # Try to disable automatic clearing of the local state.
    self.TryToDisableLocalStateAutoClearingOnChromeOS()
    self._local_state_auto_clearing = \
        self.IsLocalStateAutoClearingEnabledOnChromeOS()
    if not self._local_state_auto_clearing:
      # Prevent the inherited Logout() method from cleaning up /home/chronos
      # as this also clears the local state.
      self.set_clear_profile(False)

  def ExtraChromeFlags(self):
    """Sets up Chrome to skip OOBE.

    TODO(bartfab): Ensure OOBE is still skipped when crosbug.com/20709 is fixed.
    Disabling automatic clearing of the local state has the curious side effect
    of removing a flag that disables OOBE. This method adds back the flag.
    """
    flags = policy_base.PolicyTestBase.ExtraChromeFlags(self)
    flags.append('--login-screen=login')
    return flags

  def setUp(self):
    policy_base.PolicyTestBase.setUp(self)
    # TODO(bartfab): Remove this after crosbug.com/20709 is fixed.
    self._local_state_auto_clearing = \
        self.IsLocalStateAutoClearingEnabledOnChromeOS()

    # Cache owner credentials for easy lookup.
    credentials = self.GetPrivateInfo()['prod_enterprise_test_user']
    self._owner = credentials['username']
    self._password = credentials['password']

  def tearDown(self):
    # TODO(bartfab): Remove this after crosbug.com/20709 is fixed.
    # Try to re-enable automatic clearing of the local state and /home/chronos.
    if not self._local_state_auto_clearing:
      self.TryToEnableLocalStateAutoClearingOnChromeOS()
      self.set_clear_profile(True)
    policy_base.PolicyTestBase.tearDown(self)

  def _CheckGuestModeAvailableInLoginWindow(self):
    return self.ExecuteJavascriptInOOBEWebUI(
        """window.domAutomationController.send(
               !document.getElementById('guestSignin').hidden);""")

  def _CheckGuestModeAvailableInAccountPicker(self):
    return self.ExecuteJavascriptInOOBEWebUI(
        """window.domAutomationController.send(
               !!document.getElementById('pod-row').getPodWithUsername_(''));
               """)

  def testGuestModeEnabled(self):
    self._SetDevicePolicyAndOwner({'guest_mode_enabled': True})
    self.assertTrue(self._CheckGuestModeAvailableInLoginWindow(),
                    msg='Expected guest mode to be available.')
    self.Login(as_guest=True)
    self.Logout()

    self._SetDevicePolicyAndOwner({'guest_mode_enabled': False})
    self.assertFalse(self._CheckGuestModeAvailableInLoginWindow(),
                     msg='Expected guest mode to not be available.')

    # TODO(bartfab): Remove this after crosbug.com/20709 is fixed.
    self.TryToDisableLocalStateAutoClearing()
    if self._local_state_auto_clearing:
      logging.warn("""Unable to disable local state clearing. Skipping remainder
                      of test.""")
      return

    # Log in as a regular so that the pod row contains at least one pod and the
    # account picker is shown.
    self.Login(as_guest=False)
    self.Logout()

    self._SetDevicePolicyAndOwner({'guest_mode_enabled': True})
    self.assertTrue(self._CheckGuestModeAvailableInAccountPicker(),
                    msg='Expected guest mode to be available.')
    self.Login(as_guest=True)
    self.Logout()

    self._SetDevicePolicyAndOwner({'guest_mode_enabled': False})
    self.assertFalse(self._CheckGuestModeAvailableInAccountPicker(),
                     msg='Expected guest mode to not be available.')


if __name__ == '__main__':
  pyauto_functional.Main()
