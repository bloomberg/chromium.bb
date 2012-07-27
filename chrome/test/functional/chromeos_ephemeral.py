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
  """Tests a policy that makes users ephemeral.

  When this policy is enabled, no persistent information in the form of
  cryptohome shadow directories or local state prefs should be created for
  users. Additionally, any persistent information previously accumulated should
  be cleared when a user first logs in after enabling the policy."""

  _usernames = ('alice@example.com', 'bob@example.com')

  def _SetEphemeralUsersEnabled(self, enabled):
    """Sets the ephemeral users device policy.

    The show_user_names policy is set to False to ensure that even if the local
    state is not being automatically cleared, the login screen never shows user
    pods. This is required by the Login browser automation call.
    """
    self.SetDevicePolicy({'ephemeral_users_enabled': enabled,
                          'show_user_names': False})

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
    """Convenience method to login to the usr at the given index."""
    self.assertFalse(self.GetLoginInfo()['is_logged_in'],
                     msg='Expected to be logged out.')
    policy_base.PolicyTestBase.Login(self,
                                     self._usernames[user_index],
                                     'dummy_password')
    self.assertTrue(self.GetLoginInfo()['is_logged_in'],
                    msg='Expected to be logged in.')

  def testEnablingBeforeSession(self):
    """Checks that a new session can be made ephemeral."""
    self.PrepareToWaitForLoginFormReload()
    self._SetEphemeralUsersEnabled(True)
    self.WaitForLoginFormReload()

    self.Login(user_index=0)
    self._AssertLocalStatePrefsEmpty()
    self._AssertVaultMounted(user_index=0, ephemeral=True)
    self.Logout()

    self._AssertLocalStatePrefsEmpty()
    self._AssertNoVaultMounted()
    self._AssertVaultDirectoryDoesNotExist(user_index=0)

  def testEnablingDuringSession(self):
    """Checks that an existing non-ephemeral session is not made ephemeral."""
    self.PrepareToWaitForLoginFormReload()
    self._SetEphemeralUsersEnabled(False)
    self.WaitForLoginFormReload()

    self.Login(user_index=0)
    self._AssertLocalStatePrefsSet(user_indexes=[0])
    self._AssertVaultMounted(user_index=0, ephemeral=False)
    self._SetEphemeralUsersEnabled(True)
    self._AssertLocalStatePrefsSet(user_indexes=[0])
    self._AssertVaultMounted(user_index=0, ephemeral=False)
    self.Logout()

    self._AssertLocalStatePrefsEmpty()
    self._AssertNoVaultMounted()
    self._AssertVaultDirectoryDoesNotExist(user_index=0)

  def testDisablingDuringSession(self):
    """Checks that an existing ephemeral session is not made non-ephemeral."""
    self.PrepareToWaitForLoginFormReload()
    self._SetEphemeralUsersEnabled(True)
    self.WaitForLoginFormReload()

    self.Login(user_index=0)
    self._AssertVaultMounted(user_index=0, ephemeral=True)
    self._SetEphemeralUsersEnabled(False)
    self._AssertVaultMounted(user_index=0, ephemeral=True)
    self.Logout()

    self._AssertLocalStatePrefsEmpty()
    self._AssertNoVaultMounted()
    self._AssertVaultDirectoryDoesNotExist(user_index=0)

  def testEnablingEphemeralUsersCleansUp(self):
    """Checks that persistent information is cleared."""
    self.PrepareToWaitForLoginFormReload()
    self._SetEphemeralUsersEnabled(False)
    self.WaitForLoginFormReload()

    self.Login(user_index=0)
    self.Logout()
    self._AssertLocalStatePrefsSet(user_indexes=[0])

    self.Login(user_index=1)
    self.Logout()
    self._AssertLocalStatePrefsSet(user_indexes=[0, 1])

    self._AssertVaultDirectoryExists(user_index=0)
    self._AssertVaultDirectoryExists(user_index=1)

    self._SetEphemeralUsersEnabled(True)

    self.Login(user_index=0)
    self._AssertVaultMounted(user_index=0, ephemeral=True)
    self.Logout()

    self._AssertVaultDirectoryDoesNotExist(user_index=0)
    self._AssertVaultDirectoryDoesNotExist(user_index=1)


if __name__ == '__main__':
  pyauto_functional.Main()
