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
  """Tests user policies and their impact on the prefs UI."""

  settings_pages = [
      'chrome://settings-frame',
      'chrome://settings-frame/searchEngines',
      'chrome://settings-frame/passwords',
      'chrome://settings-frame/autofill',
      'chrome://settings-frame/content',
      'chrome://settings-frame/homePageOverlay',
      'chrome://settings-frame/languages',
  ]
  if pyauto.PyUITest.IsChromeOS():
    settings_pages += [
        'chrome://settings-frame/accounts',
    ]

  def setUp(self):
    policy_base.PolicyTestBase.setUp(self)
    if self.IsChromeOS():
      self.LoginWithTestAccount()

  def IsAnyBannerVisible(self):
    """Returns true if any banner (e.g. for managed prefs) is visible."""
    ret = self.ExecuteJavascript("""
        var visible = false;
        var banners = document.querySelectorAll('.page-banner');
        for (var i=0; i<banners.length; i++) {
          if (banners[i].parentElement.id == 'templates')
            continue;

          if (window.getComputedStyle(banners[i]).display != 'none')
            visible = true;
        }
        domAutomationController.send(visible.toString());
    """)
    return ret == 'true'

  def testNoUserPoliciesNoBanner(self):
    """Verifies the banner isn't present when no user policies are in place."""

    self.SetUserPolicy({})
    for page in PolicyPrefsUITest.settings_pages:
      self.NavigateToURL(page)
      self.assertFalse(self.IsAnyBannerVisible(), msg=
          'Unexpected banner in %s.\n'
          'Please check that chrome/test/functional/policy_prefs_ui.py has an '
          'entry for any new policies introduced.' % page)

  def RunUserPoliciesShowBanner(self, include_expected, include_unexpected):
    """Tests all the user policies on each settings page.

    If |include_expected|, pages where the banner is expected will be verified.
    If |include_unexpected|, pages where the banner should not appear will also
    be verified. This can take some time.
    """

    os = self.GetPlatform()
    all_policies = self.GetPolicyDefinitionList()

    for policy, policy_test in PolicyPrefsTestCases.policies.iteritems():
      # Skip device policies
      if policy in all_policies and all_policies[policy][1]:
        continue
      if os not in policy_test[PolicyPrefsTestCases.INDEX_OS]:
        continue
      expected_pages = [PolicyPrefsUITest.settings_pages[n]
                        for n in policy_test[PolicyPrefsTestCases.INDEX_PAGES]]
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
            policy: policy_test[PolicyPrefsTestCases.INDEX_VALUE]
          }
          self.SetUserPolicy(policy_dict)
        self.NavigateToURL(page)
        self.assertEqual(expected, self.IsAnyBannerVisible(), msg=
            'Banner was%sexpected in %s, but it was%svisible.\n'
            'The policy tested was "%s".\n'
            'Please check that chrome/test/functional/policy_prefs_ui.py has '
            'an entry for any new policies introduced.' %
                (expected and ' ' or ' NOT ', page, expected and ' NOT ' or ' ',
                 policy))
      if did_test:
        logging.debug('Policy passed: %s' % policy)

  def testUserPoliciesShowBanner(self):
    """Verifies the banner is shown when a user pref is managed by policy."""
    self.RunUserPoliciesShowBanner(True, False)

  # This test is disabled by default because it takes a very long time,
  # for little benefit.
  def UserPoliciesDontShowBanner(self):
    """Verifies that the banner is NOT shown on unrelated pages."""
    self.RunUserPoliciesShowBanner(False, True)

  def testFailOnUserPoliciesNotTested(self):
    """Verifies that all existing user policies are covered.

    Fails for all user policies listed in GetPolicyDefinitionList() that aren't
    listed in |PolicyPrefsUITest.policies|, and thus are not tested by
    |testUserPoliciesShowBanner|.
    """

    all_policies = self.GetPolicyDefinitionList()
    for policy in all_policies:
      # Skip device policies
      if all_policies[policy][1]:
        continue
      self.assertTrue(policy in PolicyPrefsTestCases.policies, msg=
          'Policy "%s" does not have a test in '
          'chrome/test/functional/policy_prefs_ui.py.\n'
          'Please edit the file and add an entry for this policy.' % policy)
      test_type = type(PolicyPrefsTestCases.policies[policy]
                       [PolicyPrefsTestCases.INDEX_VALUE]).__name__
      expected_type = all_policies[policy][0]
      self.assertEqual(expected_type, test_type, msg=
          'Policy "%s" has type "%s" but the test value has type "%s".' %
              (policy, expected_type, test_type))

  def testToggleUserPolicyTogglesBanner(self):
    """Verifies that toggling a user policy toggles the banner's visibility."""
    # |policy| just has to be any user policy that has at least a settings page
    # that displays the banner when the policy is set.
    policy = 'ShowHomeButton'

    policy_test = PolicyPrefsTestCases.policies[policy]
    page = PolicyPrefsUITest.settings_pages[
        policy_test[PolicyPrefsTestCases.INDEX_PAGES][0]]
    policy_dict = {
      policy: policy_test[PolicyPrefsTestCases.INDEX_VALUE]
    }

    self.SetUserPolicy({})
    self.NavigateToURL(page)
    self.assertFalse(self.IsAnyBannerVisible())

    self.SetUserPolicy(policy_dict)
    self.assertTrue(self.IsAnyBannerVisible())

    self.SetUserPolicy({})
    self.assertFalse(self.IsAnyBannerVisible())


if __name__ == '__main__':
  pyauto_functional.Main()
