#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# If this test is failing, please check these steps.
#
# - You introduced a new policy:
#   Cool! Edit |policies| in policy_test_cases.py and add an entry for it.
#   See the comment above it for the format.
#
# - This test fails after your changes:
#   Did you change the chrome://settings WebUI and edited "pref" attributes?
#   Make sure any preferences that can be managed by policy have a "pref"
#   attribute. Ping joaodasilva@chromium.org or bauerb@chromium.org.
#
# - This test fails for other reasons, or is flaky:
#   Ping joaodasilva@chromium.org, pastarmovj@chromium.org or
#   mnissler@chromium.org.

import logging

import pyauto_functional  # must come before pyauto.
import policy_base
import pyauto

from policy_test_cases import PolicyPrefsTestCases


class PolicyPrefsUITest(policy_base.PolicyTestBase):
  """Tests policies and their impact on the prefs UI."""

  settings_pages = [
      'chrome://settings-frame',
      'chrome://settings-frame/searchEngines',
      'chrome://settings-frame/passwords',
      'chrome://settings-frame/autofill',
      'chrome://settings-frame/content',
      'chrome://settings-frame/languages',
  ]
  if pyauto.PyUITest.IsChromeOS():
    settings_pages += [
        'chrome://settings-frame/accounts',
    ]

  def GetPlatform(self):
    if self.IsChromeOS():
      return 'chromeos'
    elif self.IsLinux():
      return 'linux'
    elif self.IsMac():
      return 'mac'
    elif self.IsWin():
      return 'win'
    else:
      self.fail(msg='Unknown platform')

  def IsBannerVisible(self):
    """Returns true if the managed-banner is visible in the current page."""
    # TODO(csilv): This logic assumes there is only one banner, it needs to be
    # updated to work with uber page.
    ret = self.ExecuteJavascript("""
        var visible = false;
        var banner = document.getElementById('managed-prefs-banner');
        if (banner)
          visible = !banner.hidden;
        domAutomationController.send(visible.toString());
    """)
    return ret == 'true'

  def testNoPoliciesNoBanner(self):
    """Verifies that the banner isn't present when no policies are in place."""

    self.SetPolicies({})
    for page in PolicyPrefsUITest.settings_pages:
      self.NavigateToURL(page)
      self.assertFalse(self.IsBannerVisible(), msg=
          'Unexpected banner in %s.\n'
          'Please check that chrome/test/functional/policy_prefs_ui.py has an '
          'entry for any new policies introduced.' % page)

  def RunPoliciesShowBanner(self, include_expected, include_unexpected):
    """Tests all the policies on each settings page.

    If |include_expected|, pages where the banner is expected will be verified.
    If |include_unexpected|, pages where the banner should not appear will also
    be verified. This can take some time.
    """

    os = self.GetPlatform()

    for policy in PolicyPrefsTestCases.policies:
      policy_test = PolicyPrefsTestCases.policies[policy]
      if len(policy_test) >= 3 and not os in policy_test[2]:
        continue
      expected_pages = [ PolicyPrefsUITest.settings_pages[n]
                         for n in policy_test[1] ]
      did_test = False
      for page in PolicyPrefsUITest.settings_pages:
        expected = page in expected_pages
        if expected and not include_expected:
          continue
        if not expected and not include_unexpected:
          continue
        if not did_test:
          did_test = True
          policy_dict = {
            policy: policy_test[0]
          }
          self.SetPolicies(policy_dict)
        self.NavigateToURL(page)
        self.assertEqual(expected, self.IsBannerVisible(), msg=
            'Banner was%sexpected in %s, but it was%svisible.\n'
            'The policy tested was "%s".\n'
            'Please check that chrome/test/functional/policy_prefs_ui.py has '
            'an entry for any new policies introduced.' %
                (expected and ' ' or ' NOT ', page, expected and ' NOT ' or ' ',
                 policy))
      if did_test:
        logging.debug('Policy passed: %s' % policy)

  def testPoliciesShowBanner(self):
    """Verifies that the banner is shown when a pref is managed by policy."""
    self.RunPoliciesShowBanner(True, False)

  # This test is disabled by default because it takes a very long time,
  # for little benefit.
  def PoliciesDontShowBanner(self):
    """Verifies that the banner is NOT shown on unrelated pages."""
    self.RunPoliciesShowBanner(False, True)

  def testFailOnPoliciesNotTested(self):
    """Verifies that all existing policies are covered.

    Fails for all policies listed in GetPolicyDefinitionList() that aren't
    listed in |PolicyPrefsUITest.policies|, and thus are not tested by
    |testPoliciesShowBanner|.
    """

    all_policies = self.GetPolicyDefinitionList()
    for policy in all_policies:
      self.assertTrue(policy in PolicyPrefsTestCases.policies, msg=
          'Policy "%s" does not have a test in '
          'chrome/test/functional/policy_prefs_ui.py.\n'
          'Please edit the file and add an entry for this policy.' % policy)
      test_type = type(PolicyPrefsTestCases.policies[policy][0]).__name__
      expected_type = all_policies[policy]
      self.assertEqual(expected_type, test_type, msg=
          'Policy "%s" has type "%s" but the test value has type "%s".' %
              (policy, expected_type, test_type))

  def testTogglePolicyTogglesBanner(self):
    """Verifies that toggling a policy also toggles the banner's visitiblity."""
    # |policy| just has to be any policy that has at least a settings page that
    # displays the banner when the policy is set.
    policy = 'ShowHomeButton'

    policy_test = PolicyPrefsTestCases.policies[policy]
    page = PolicyPrefsUITest.settings_pages[policy_test[1][0]]
    policy_dict = {
      policy: policy_test[0]
    }

    self.SetPolicies({})
    self.NavigateToURL(page)
    self.assertFalse(self.IsBannerVisible())

    self.SetPolicies(policy_dict)
    self.assertTrue(self.IsBannerVisible())

    self.SetPolicies({})
    self.assertFalse(self.IsBannerVisible())


if __name__ == '__main__':
  pyauto_functional.Main()
