#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

import pyauto_functional  # must come before pyauto.
import policy_base
import policy_test_cases
import pyauto_errors
import pyauto


class PolicyTest(policy_base.PolicyTestBase):
  """Tests that the effects of policies are being enforced as expected."""

  def _GetPrefIsManagedError(self, pref, expected_value):
    """Verify the managed preferences cannot be modified.

    Args:
      pref: The preference key that you want to modify.
      expected_value: Current value of the preference.

    Returns:
      Error message if any, None if pref is successfully managed.
    """
    # Check if the current value of the preference is set as expected.
    local_state_pref_value = self.GetLocalStatePrefsInfo().Prefs(pref)
    profile_pref_value = self.GetPrefsInfo().Prefs(pref)
    actual_value = (profile_pref_value if profile_pref_value is not None else
                    local_state_pref_value)
    if actual_value is None:
      return 'Preference %s is not registered.'  % pref
    elif actual_value != expected_value:
      return ('Preference value "%s" does not match policy value "%s".' %
              (actual_value, expected_value))
    # If the preference is managed, this should throw an exception.
    try:
      if profile_pref_value is not None:
        self.SetPrefs(pref, expected_value)
      else:
        self.SetLocalStatePrefs(pref, expected_value)
    except pyauto_errors.JSONInterfaceError, e:
      if str(e) != 'pref is managed. cannot be changed.':
        return str(e)
      else:
        return None
    else:
      return 'Preference can be set even though a policy is in effect.'

  def setUp(self):
    policy_base.PolicyTestBase.setUp(self)
    if self.IsChromeOS():
      self.LoginWithTestAccount()

  def testPolicyToPrefMapping(self):
    """Verify that simple user policies map to corresponding prefs.

    Also verify that once these policies are in effect, the prefs cannot be
    modified by the user.
    """
    total = 0
    fails = []
    policy_prefs = policy_test_cases.PolicyPrefsTestCases
    for policy, values in policy_prefs.policies.iteritems():
      pref = values[policy_prefs.INDEX_PREF]
      value = values[policy_prefs.INDEX_VALUE]
      os = values[policy_prefs.INDEX_OS]
      if not pref or self.GetPlatform() not in os:
        continue
      self.SetUserPolicy({policy: value})
      error = self._GetPrefIsManagedError(getattr(pyauto, pref), value)
      if error:
        fails.append('%s: %s' % (policy, error))
      total += 1
    self.assertFalse(fails, msg='%d of %d policies failed.\n%s' %
                     (len(fails), total, '\n'.join(fails)))

  def testApplicationLocaleValue(self):
    """Verify that Chrome can be launched only in a specific locale."""
    if self.IsWin():
      policy = {'ApplicationLocaleValue': 'hi'}
      self.SetUserPolicy(policy)
      self.assertTrue(
          'hi' in self.GetPrefsInfo().Prefs()['intl']['accept_languages'],
          msg='Chrome locale is not Hindi.')
      # TODO(sunandt): Try changing the application locale to another language.
    else:
      raise NotImplementedError()


if __name__ == '__main__':
  pyauto_functional.Main()
