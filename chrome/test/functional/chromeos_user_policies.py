#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess

import pyauto_functional  # Must be imported before pyauto
import pyauto


class ChromeosUserPolicies(pyauto.PyUITest):
  """Test cases for getting correct enterprise user policies"""

  assert os.geteuid() == 0, 'Need to run this test as root'

  _POLICY_EXPECTED_FILE = os.path.join(pyauto.PyUITest.DataDir(),
                                       'chromeos', 'enterprise',
                                       'policy_user')

  # TODO(scunningham): Use test server flags for gaia authentication
  #                    and DM Server after bug 22682 is fixed.
  #_EXTRA_CHROME_FLAGS = [
  # ('--device-management-url='
  #  'https://cros-auto.sandbox.google.com/devicemanagement/data/api'),
  # '--gaia-host=gaiastaging.corp.google.com',
  #]
  _EXTRA_CHROME_FLAGS = [
    ('--device-management-url='
     'https://m.google.com/devicemanagement/data/api'),
    '--gaia-host=www.google.com',
  ]

  def setUp(self):
    """Restart session_manager for clean startup on every run.

    Overrides setUp() in pyauto.py.
    """
    assert self.WaitForSessionManagerRestart(
        lambda: subprocess.call(['pkill', 'session_manager'])), \
       'Timed out waiting for session_manager to start.'
    pyauto.PyUITest.setUp(self)

  def ExtraChromeFlags(self):
    """Launches chrome with additional flags for enterprise test.

    Overrides the default list of extra flags passed to Chrome by
    ExtraChromeFlags() in pyauto.py.
    """
    return pyauto.PyUITest.ExtraChromeFlags(self) + self._EXTRA_CHROME_FLAGS

  # TODO(scunningham): Workaround for bug 22682 is to use accounts in
  # production domain. When fixed, restore accounts to those in QA domain.
  def _LoginWithGoogleAppsDomainAccount(self, account_type):
    """Sign in with credentials for |account_type| in Google Apps domain.

    Args:
      account_type: This is a string representing one of the 5 types of user
                    accounts needed to cover every possible policy value.
                    The account types are based on the organization unit of
                    the user: Executive, Operations, Development, Test, and
                    Sales. The corresponding string values are:
                    'test_enterprise_executive_user',
                    'test_enterprise_operations_user',
                    'test_enterprise_developer_user',
                    'test_enterprise_tester_user', and
                    'test_enterprise_sales_user'
    """
    credentials = self.GetPrivateInfo()[account_type]
    self.Login(credentials['username'], credentials['password'])

  def _GetExpectedPolicyData(self):
    """Read policy data from the policy expected file."""
    assert os.path.exists(self._POLICY_EXPECTED_FILE), \
           'Expected policy data file does not exist.'
    return self.EvalDataFrom(self._POLICY_EXPECTED_FILE)

  def testExecutiveUserMandatoryPolicies(self):
    """Verify User Mandatory Policies fetched from Google Apps Domain."""
    account_type = 'test_enterprise_executive_user'
    self._LoginWithGoogleAppsDomainAccount(account_type)

    # Login and verify success.
    login_info = self.GetLoginInfo()
    self.assertTrue(login_info['is_logged_in'], msg='Login failed.')

    # Read private expected policy data.
    expect_policy_dict = self._GetExpectedPolicyData()
    expect_user = expect_policy_dict[account_type]
    self.expect = expect_user['user_mandatory_policies']

    # Fetch actual policy data.
    actual_policy_dict = self.FetchEnterprisePolicy()
    self.actual = actual_policy_dict['user_mandatory_policies']

    # Compare actual against expected policies.
    added_keys = []
    missing_keys = []
    wrong_values = []
    for policy_key in self.expect:
      if not policy_key in self.actual:
        missing_keys.append(policy_key)
        continue
      actual_value = self.actual[policy_key]
      expect_value = self.expect[policy_key]
      if actual_value != expect_value:
        wrong_values.append(policy_key)
    set_actual = set(self.actual.keys())
    set_expect = set(self.expect.keys())
    set_match = set_actual.intersection(set_expect)
    added_keys = set_actual.difference(set_match)
    self._CheckPolicyErrors(added_keys, missing_keys, wrong_values)

    self.Logout()

  def _CheckPolicyErrors(self, added, missing, wrong):
    """Check for policy errors. If any, report and assert failure.

    Args:
      added: Set of added policy keys.
      missing: Set of omitted policy keys.
      wrong: Set of policy keys with unexpected values.
    """
    if added or missing or wrong:
      err = ''
      if added:
        err += '  Added key(s): %s\n' % added
      if missing:
        err += '  Missing keys(s): %s\n' % missing
      if wrong:
        err += '  Wrong value(s):\n'
        for key in wrong:
          err += '    Key: %s (Expected: %s, Actual: %s)\n' % \
          (key, self.expect[key], self.actual[key])
      self.fail('Unexpected policy error:\n%s' % err)


if __name__ == '__main__':
  pyauto_functional.Main()
