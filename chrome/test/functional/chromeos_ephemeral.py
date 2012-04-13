# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

import pyauto_functional  # Must come before pyauto (and thus, policy_base).
import policy_base

sys.path.append('/usr/local')  # Required to import autotest libs.
from autotest.cros import constants
from autotest.cros import cryptohome


class ChromeosEphemeral(policy_base.PolicyTestBase):
  """Tests a policy that makes all users except the owner ephemeral.

  When this policy is enabled, no persistent information in the form of
  cryptohome shadow directories or local state prefs should be created for
  users. Additionally, any persistent information previously accumulated should
  be cleared when a user first logs in after enabling the policy."""

  def _SetDevicePolicyAndOwner(self, ephemeral_users_enabled, owner_index):
    """Sets device policy and owner.

    TODO(bartfab): Ensure Login still works after crosbug.com/20709 is fixed.
    The show_user_names policy is set to False to ensure that even if the local
    state is not being automatically cleared, the login screen never shows user
    pods. This is required by the Login browser automation call.
    """
    self.SetDevicePolicy(
        device_policy={'ephemeral_users_enabled': ephemeral_users_enabled,
                       'show_user_names': False},
        owner=self._usernames[owner_index])

  def _DoesVaultDirectoryExist(self, user_index):
    user_hash = cryptohome.get_user_hash(self._usernames[user_index])
    return os.path.exists(os.path.join(constants.SHADOW_ROOT, user_hash))

  def _AssertLocalStatePrefsSet(self, user_indexes):
    expected = sorted([self._usernames[index] for index in user_indexes])
    # The OAuthTokenStatus pref is populated asynchronously. Checking whether it
    # is set would lead to an ugly race.
    for pref in ['LoggedInUsers', 'UserImages', 'UserDisplayEmail', ]:
      actual = sorted(self.GetLocalStatePrefsInfo().Prefs(pref))
      self.assertEqual(actual, expected,
                       msg='Expected to find prefs in local state for users.')

  def _AssertLocalStatePrefsEmpty(self):
    for pref in ['LoggedInUsers',
                 'UserImages',
                 'UserDisplayEmail',
                 'OAuthTokenStatus']:
      self.assertFalse(self.GetLocalStatePrefsInfo().Prefs(pref),
          msg='Expected to not find prefs in local state for any user.')

  def _AssertVaultDirectoryExists(self, user_index):
    self.assertTrue(self._DoesVaultDirectoryExist(user_index=user_index),
                    msg='Expected vault shadow directory to exist.')

  def _AssertVaultDirectoryDoesNotExist(self, user_index):
    self.assertFalse(self._DoesVaultDirectoryExist(user_index=user_index),
                     msg='Expected vault shadow directory to not exist.')

  def _AssertVaultMounted(self, user_index, ephemeral):
    if ephemeral:
      device_regex = constants.CRYPTOHOME_DEV_REGEX_REGULAR_USER_EPHEMERAL
      fs_regex = constants.CRYPTOHOME_FS_REGEX_TMPFS
    else:
      device_regex = constants.CRYPTOHOME_DEV_REGEX_REGULAR_USER_SHADOW
      fs_regex = constants.CRYPTOHOME_FS_REGEX_ANY
    self.assertTrue(
        cryptohome.is_vault_mounted(device_regex=device_regex,
                                    fs_regex=fs_regex,
                                    user=self._usernames[user_index],
                                    allow_fail=True),
        msg='Expected vault backed by %s to be mounted.' %
            'tmpfs' if ephemeral else 'shadow directory')

  def _AssertNoVaultMounted(self):
    self.assertFalse(cryptohome.is_vault_mounted(allow_fail=True),
                     msg='Did not expect any vault to be mounted.')

  def Login(self, user_index):
    self.assertFalse(self.GetLoginInfo()['is_logged_in'],
                     msg='Expected to be logged out.')
    policy_base.PolicyTestBase.Login(self,
                                     self._usernames[user_index],
                                     self._passwords[user_index])
    self.assertTrue(self.GetLoginInfo()['is_logged_in'],
                    msg='Expected to be logged in.')

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
    # Try to disable automatic clearing of the local state.
    self.TryToDisableLocalStateAutoClearingOnChromeOS()
    self._local_state_auto_clearing = \
        self.IsLocalStateAutoClearingEnabledOnChromeOS()
    if not self._local_state_auto_clearing:
      # Prevent the inherited Logout() method from cleaning up /home/chronos
      # as this also clears the local state.
      self.set_clear_profile(False)

    credentials = (self.GetPrivateInfo()['prod_enterprise_test_user'],
                   self.GetPrivateInfo()['prod_enterprise_executive_user'],
                   self.GetPrivateInfo()['prod_enterprise_sales_user'])
    self._usernames = [credential['username'] for credential in credentials]
    self._passwords = [credential['password'] for credential in credentials]

  def tearDown(self):
    # TODO(bartfab): Remove this after crosbug.com/20709 is fixed.
    # Try to re-enable automatic clearing of the local state and /home/chronos.
    if not self._local_state_auto_clearing:
      self.TryToEnableLocalStateAutoClearingOnChromeOS()
      self.set_clear_profile(True)
    policy_base.PolicyTestBase.tearDown(self)

  def testLoginAsOwnerIsNotEphemeral(self):
    """Checks that the owner does not become ephemeral."""
    self._SetDevicePolicyAndOwner(ephemeral_users_enabled=True, owner_index=0)

    self.Login(user_index=0)
    # TODO(bartfab): Remove this when crosbug.com/20709 is fixed.
    if self._local_state_auto_clearing:
      self._AssertLocalStatePrefsSet(user_indexes=[0])
    self._AssertVaultDirectoryExists(user_index=0)
    self._AssertVaultMounted(user_index=0, ephemeral=False)
    self.Logout()
    # TODO(bartfab): Make this unconditional when crosbug.com/20709 is fixed.
    if not self._local_state_auto_clearing:
      self._AssertLocalStatePrefsSet(user_indexes=[0])
    self._AssertVaultDirectoryExists(user_index=0)
    self._AssertNoVaultMounted()

  def testLoginAsNonOwnerIsEphemeral(self):
    """Checks that a non-owner user does become ephemeral."""
    self._SetDevicePolicyAndOwner(ephemeral_users_enabled=True, owner_index=0)

    self.Login(user_index=1)
    # TODO(bartfab): Remove this when crosbug.com/20709 is fixed.
    if self._local_state_auto_clearing:
      self._AssertLocalStatePrefsEmpty()
    self._AssertVaultDirectoryDoesNotExist(user_index=1)
    self._AssertVaultMounted(user_index=1, ephemeral=True)
    self.Logout()
    # TODO(bartfab): Make this unconditional when crosbug.com/20709 is fixed.
    if not self._local_state_auto_clearing:
      self._AssertLocalStatePrefsEmpty()

    self._AssertVaultDirectoryDoesNotExist(user_index=1)
    self._AssertNoVaultMounted()

  def testEnablingEphemeralUsersCleansUp(self):
    """Checks that persistent information is cleared."""
    self._SetDevicePolicyAndOwner(ephemeral_users_enabled=False, owner_index=0)

    self.Login(user_index=0)
    # TODO(bartfab): Remove this when crosbug.com/20709 is fixed.
    if self._local_state_auto_clearing:
      self._AssertLocalStatePrefsSet(user_indexes=[0])
    self.Logout()
    # TODO(bartfab): Make this unconditional when crosbug.com/20709 is fixed.
    if not self._local_state_auto_clearing:
      self._AssertLocalStatePrefsSet(user_indexes=[0])

    self.Login(user_index=1)
    # TODO(bartfab): Remove this when crosbug.com/20709 is fixed.
    if self._local_state_auto_clearing:
      self._AssertLocalStatePrefsSet(user_indexes=[1])
    self.Logout()
    # TODO(bartfab): Make this unconditional when crosbug.com/20709 is fixed.
    if not self._local_state_auto_clearing:
      self._AssertLocalStatePrefsSet(user_indexes=[0, 1])

    self.Login(user_index=2)
    # TODO(bartfab): Remove this when crosbug.com/20709 is fixed.
    if self._local_state_auto_clearing:
      self._AssertLocalStatePrefsSet(user_indexes=[2])
    self.Logout()
    # TODO(bartfab): Make this unconditional when crosbug.com/20709 is fixed.
    if not self._local_state_auto_clearing:
      self._AssertLocalStatePrefsSet(user_indexes=[0, 1, 2])

    self._AssertVaultDirectoryExists(user_index=0)
    self._AssertVaultDirectoryExists(user_index=1)
    self._AssertVaultDirectoryExists(user_index=2)

    self._SetDevicePolicyAndOwner(ephemeral_users_enabled=True, owner_index=0)

    self.Login(user_index=1)
    # TODO(bartfab): Remove this when crosbug.com/20709 is fixed.
    if self._local_state_auto_clearing:
      self._AssertLocalStatePrefsEmpty()
    self._AssertVaultMounted(user_index=1, ephemeral=True)
    self.Logout()

    # TODO(bartfab): Make this unconditional when crosbug.com/20709 is fixed.
    if not self._local_state_auto_clearing:
      self._AssertLocalStatePrefsSet(user_indexes=[0])

    self._AssertVaultDirectoryExists(user_index=0)
    self._AssertVaultDirectoryDoesNotExist(user_index=1)
    self._AssertVaultDirectoryDoesNotExist(user_index=2)


if __name__ == '__main__':
  pyauto_functional.Main()
