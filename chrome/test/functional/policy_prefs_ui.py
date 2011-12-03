#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# If this test is failing, please check these steps.
#
# - You introduced a new policy:
#   Cool! Edit |policies| below and add an entry for it. See the comment above
#   it for the format.
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


class PolicyPrefsUITest(policy_base.PolicyTestBase):
  BROWSER         = 0
  PERSONAL        = 1
  ADVANCED        = 2
  SEARCH_ENGINES  = 3
  PASSWORDS       = 4
  AUTOFILL        = 5
  CONTENT         = 6
  LANGUAGES       = 7
  EXTENSIONS      = 8
  SYSTEM          = 9
  INTERNET        = 10
  ACCOUNTS        = 11

  settings_pages = [
      'chrome://settings/browser',
      'chrome://settings/personal',
      'chrome://settings/advanced',
      'chrome://settings/searchEngines',
      'chrome://settings/passwords',
      'chrome://settings/autofill',
      'chrome://settings/content',
      'chrome://settings/languages',
      'chrome://settings/extensions',
  ]
  if pyauto.PyUITest.IsChromeOS():
    settings_pages += [
        'chrome://settings/system',
        'chrome://settings/internet',
        'chrome://settings/accounts',
    ]

  # Each policy has an entry with a tuple (Value, Pages, OS)
  #
  # |Value| is the value the policy should have when it is enforced.
  #
  # |Pages| is a list with integer indices into |settings_pages|, and lists all
  #         the pages that should display the managed-banner.
  #         Leave empty if this policy doesn't display anything visible in
  #         the settings.
  #
  # |OS| is a list of platforms where this policy should be tested. When empty,
  #      all platforms are tested. Valid platforms are 'win', 'mac', 'linux'
  #      and 'chromeos'.
  policies = {
    'HomepageLocation': ('http://chromium.org', [ BROWSER ]),
    'HomepageIsNewTabPage': (True, [ BROWSER ]),
    # TODO(joaodasilva): Couldn't verify on linux.
    'DefaultBrowserSettingEnabled': (False, [], [ 'win', 'mac', 'linux' ]),
    # TODO(joaodasilva): Test this on windows.
    'ApplicationLocaleValue': ('', [], [ 'win' ]),
    'AlternateErrorPagesEnabled': (False, [ ADVANCED ]),
    'SearchSuggestEnabled': (False, [ ADVANCED ]),
    'DnsPrefetchingEnabled': (False, [ ADVANCED ]),
    'DisableSpdy': (True, []),
    'DisabledSchemes': ( ['file' ], []),
    'JavascriptEnabled': (False, [ CONTENT ]),
    'IncognitoEnabled': (False, []),
    'IncognitoModeAvailability': (1, []),
    'SavingBrowserHistoryDisabled': (True, []),
    'RemoteAccessClientFirewallTraversal': (True, []),
    'RemoteAccessHostFirewallTraversal': (True, []),
    'PrintingEnabled': (False, []),
    # Note: supported_on is empty for this policy.
    'CloudPrintProxyEnabled': (True, [], []),
    'CloudPrintSubmitEnabled': (False, [], ['win', 'mac', 'linux']),
    'SafeBrowsingEnabled': (False, [ ADVANCED ]),
    # TODO(joaodasilva): This is only in place on official builds, but the
    # SetUserCloudPolicy call is a nop on official builds. Should be ADVANCED.
    'MetricsReportingEnabled': (False, []),
    'PasswordManagerEnabled': (False, [ PERSONAL ]),
    # TODO(joaodasilva): Should be PASSWORDS too. http://crbug.com/97749
    'PasswordManagerAllowShowPasswords': (False, [ PERSONAL ]),
    'AutoFillEnabled': (False, [ PERSONAL ]),
    'DisabledPlugins': (['Flash'], []),
    'EnabledPlugins': (['Flash'], []),
    'DisabledPluginsExceptions': (['Flash'], []),
    'DisablePluginFinder': (True, []),
    # TODO(joaodasilva): Should be PERSONAL. http://crbug.com/97749
    'SyncDisabled': (True, []),
    'UserDataDir': ('${users}/${user_name}/chrome-test', [], [ 'win', 'mac' ]),
    'DiskCacheDir': ('${user_home}/test-cache', [], [ 'win', 'mac', 'linux' ]),
    'DiskCacheSize': (100, [], [ 'win', 'mac', 'linux' ]),
    'MediaCacheSize': (200, [], [ 'win', 'mac', 'linux' ]),
    'DownloadDirectory': ('${user_home}/test-downloads', [ ADVANCED ],
                          [ 'win', 'mac', 'linux' ]),
    'ClearSiteDataOnExit': (True, [ CONTENT ]),
    # TODO(joaodasilva): Should be ADVANCED. http://crbug.com/97749
    'ProxyMode': ('direct', [], [ 'win', 'mac', 'linux' ]),
    # TODO(joaodasilva): Should be ADVANCED. http://crbug.com/97749
    'ProxyServerMode': (0, [], [ 'win', 'mac', 'linux' ]),
    'ProxyServer': ('http://localhost:8080', [], [ 'win', 'mac', 'linux' ]),
    'ProxyPacUrl': ('http://localhost:8080/proxy.pac', [],
                    [ 'win', 'mac', 'linux' ]),
    'ProxyBypassList': ('localhost', [], [ 'win', 'mac', 'linux' ]),
    'EnableOriginBoundCerts': (False, []),
    'AuthSchemes': ('AuthSchemes', []),
    'DisableAuthNegotiateCnameLookup': (True, []),
    'EnableAuthNegotiatePort': (False, []),
    'AuthServerWhitelist': ('localhost', []),
    'AuthNegotiateDelegateWhitelist': ('localhost', []),
    'GSSAPILibraryName': ('libwhatever.so', [], [ 'mac', 'linux' ]),
    'AllowCrossOriginAuthPrompt': (False, [], [ 'win', 'mac', 'linux' ]),
    'ExtensionInstallBlacklist': ( ['*'], []),
    'ExtensionInstallWhitelist': ( ['lcncmkcnkcdbbanbjakcencbaoegdjlp' ], []),
    'ExtensionInstallForcelist': (
        ['lcncmkcnkcdbbanbjakcencbaoegdjlp;' +
         'https://clients2.google.com/service/update2/crx' ], []),
    'ShowHomeButton': (True, [ BROWSER ]),
    'DeveloperToolsDisabled': (True, []),
    'RestoreOnStartup': (0, [ BROWSER ]),
    # TODO(joaodasilva): Should be BROWSER. http://crbug.com/97749
    'RestoreOnStartupURLs': ([ 'chromium.org' ], []),
    # TODO(joaodasilva): The banner is out of place. http://crbug.com/77791
    'BlockThirdPartyCookies': (True, [ CONTENT ]),
    # TODO(joaodasilva): Should be BROWSER. http://crbug.com/97749
    'DefaultSearchProviderEnabled': (False, []),
    'DefaultSearchProviderName': ('google.com', []),
    'DefaultSearchProviderKeyword': ('google', []),
    # TODO(joaodasilva): Should be BROWSER. http://crbug.com/97749
    'DefaultSearchProviderSearchURL': ('http://www.google.com/?q={searchTerms}',
                                       []),
    'DefaultSearchProviderSuggestURL':
        ('http://www.google.com/suggest?q={searchTerms}', []),
    'DefaultSearchProviderInstantURL':
        ('http://www.google.com/instant?q={searchTerms}', []),
    'DefaultSearchProviderIconURL': ('http://www.google.com/favicon.ico', []),
    'DefaultSearchProviderEncodings': ([ 'UTF-8' ], []),
    'DefaultCookiesSetting': (2, [ CONTENT ]),
    'DefaultImagesSetting': (2, [ CONTENT ]),
    'DefaultJavaScriptSetting': (2, [ CONTENT ]),
    'DefaultPluginsSetting': (2, [ CONTENT ]),
    'DefaultPopupsSetting': (2, [ CONTENT ]),
    'DefaultNotificationsSetting': (2, [ CONTENT ]),
    'DefaultGeolocationSetting': (2, [ CONTENT ]),
    'AutoSelectCertificateForUrls':
        ([ '{\'pattern\':\'https://example.com\',' +
           '\'filter\':{\'ISSUER\':{\'CN\': \'issuer-name\'}}}' ], []),
    'CookiesAllowedForUrls': ([ '[*.]google.com' ], []),
    'CookiesBlockedForUrls': ([ '[*.]google.com' ], []),
    'CookiesSessionOnlyForUrls': ([ '[*.]google.com' ], []),
    'ImagesAllowedForUrls': ([ '[*.]google.com' ], []),
    'ImagesBlockedForUrls': ([ '[*.]google.com' ], []),
    'JavaScriptAllowedForUrls': ([ '[*.]google.com' ], []),
    'JavaScriptBlockedForUrls': ([ '[*.]google.com' ], []),
    'PluginsAllowedForUrls': ([ '[*.]google.com' ], []),
    'PluginsBlockedForUrls': ([ '[*.]google.com' ], []),
    'PopupsAllowedForUrls': ([ '[*.]google.com' ], []),
    'PopupsBlockedForUrls': ([ '[*.]google.com' ], []),
    'NotificationsAllowedForUrls': ([ '[*.]google.com' ], []),
    'NotificationsBlockedForUrls': ([ '[*.]google.com' ], []),
    'Disable3DAPIs': (True, []),
    'InstantEnabled': (False, [ BROWSER ]),
    'TranslateEnabled': (False, [ ADVANCED ]),
    'AllowOutdatedPlugins': (False, []),
    'AlwaysAuthorizePlugins': (True, []),
    'BookmarkBarEnabled': (False, [ BROWSER ]),
    'EditBookmarksEnabled': (False, []),
    'AllowFileSelectionDialogs': (False, [ ADVANCED ],
                                  [ 'win', 'mac', 'linux' ]),
    'ImportBookmarks': (False, [], [ 'win', 'mac', 'linux' ]),
    'ImportHistory': (False, [], [ 'win', 'mac', 'linux' ]),
    'ImportHomepage': (False, [], [ 'win', 'mac', 'linux' ]),
    'ImportSearchEngine': (False, [], [ 'win', 'mac', 'linux' ]),
    'ImportSavedPasswords': (False, [], [ 'win', 'mac', 'linux' ]),
    'MaxConnectionsPerProxy': (32, []),
    'HideWebStorePromo': (True, []),
    'URLBlacklist': ([ 'google.com' ], []),
    'URLWhitelist': ([ 'google.com' ], []),
    'EnterpriseWebStoreURL': ('', []),
    'EnterpriseWebStoreName': ('', []),

    # ChromeOS-only policies:
    'ChromeOsLockOnIdleSuspend': (True, [ PERSONAL ], [ 'chromeos' ]),
    'PolicyRefreshRate': (300000, [], [ 'chromeos' ]),
    'OpenNetworkConfiguration': ('', [], [ 'chromeos' ]),

    # ChromeOS Device policies:
    'DevicePolicyRefreshRate': (300000, [], []),
    'ChromeOsReleaseChannel': ('stable-channel', [], []),
    'DeviceOpenNetworkConfiguration': ('', [], []),

    # Chrome Frame policies:
    'ChromeFrameRendererSettings': (0, [], []),
    'RenderInChromeFrameList': ([ 'google.com' ], [], []),
    'RenderInHostList': ([ 'google.com' ], [], []),
    'ChromeFrameContentTypes': ([ 'text/xml' ], [], []),
    'GCFUserDataDir': ('${user_name}/test-frame', [], []),
  }

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

    for policy in PolicyPrefsUITest.policies:
      policy_test = PolicyPrefsUITest.policies[policy]
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
      self.assertTrue(policy in PolicyPrefsUITest.policies, msg=
          'Policy "%s" does not have a test in '
          'chrome/test/functional/policy_prefs_ui.py.\n'
          'Please edit the file and add an entry for this policy.' % policy)
      test_type = type(PolicyPrefsUITest.policies[policy][0]).__name__
      expected_type = all_policies[policy]
      self.assertEqual(expected_type, test_type, msg=
          'Policy "%s" has type "%s" but the test value has type "%s".' %
              (policy, expected_type, test_type))

  def testTogglePolicyTogglesBanner(self):
    """Verifies that toggling a policy also toggles the banner's visitiblity."""
    # |policy| just has to be any policy that has at least a settings page that
    # displays the banner when the policy is set.
    policy = 'ShowHomeButton'

    policy_test = PolicyPrefsUITest.policies[policy]
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
