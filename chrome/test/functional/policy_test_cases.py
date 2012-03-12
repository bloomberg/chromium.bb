#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This data is in a separate file so that src/chrome/app/policy/PRESUBMIT.py
# can load it too without having to load pyautolib.

class PolicyPrefsTestCases(object):
  """A list of test cases for policy_prefs_ui.py."""

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
    'CloudPrintSubmitEnabled': (False, [], [ 'win', 'mac', 'linux' ]),
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
    # Note: this policy is only used internally for now.
    'ProxySettings': ({}, [], []),
    'EnableOriginBoundCerts': (False, [], [ 'win', 'mac', 'linux' ]),
    'DisableSSLRecordSplitting': (False, []),
    'EnableOnlineRevocationChecks': (False, []),
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
    'EnableMemoryInfo': (True, []),
    'DisablePrintPreview': (True, [], [ 'win', 'mac', 'linux' ]),

    # ChromeOS-only policies:
    'ChromeOsLockOnIdleSuspend': (True, [ PERSONAL ], [ 'chromeos' ]),
    'PolicyRefreshRate': (300000, [], [ 'chromeos' ]),
    'OpenNetworkConfiguration': ('', [], [ 'chromeos' ]),

    # ChromeOS Device policies:
    'DevicePolicyRefreshRate': (300000, [], []),
    'ChromeOsReleaseChannel': ('stable-channel', [], []),
    'DeviceOpenNetworkConfiguration': ('', [], []),
    'ReportDeviceVersionInfo': (True, [], []),
    'ReportDeviceActivityTimes': (True, [], []),
    'ReportDeviceBootMode': (True, [], []),
    'DeviceAllowNewUsers': (True, [], []),
    'DeviceUserWhitelist': ([], [], []),
    'DeviceGuestModeEnabled': (True, [], []),
    'DeviceShowUserNamesOnSignin': (True, [], []),
    'DeviceDataRoamingEnabled': (True, [], []),
    'DeviceMetricsReportingEnabled': (True, [], []),
    'DeviceEphemeralUsersEnabled': (True, [], []),

    # Chrome Frame policies:
    'ChromeFrameRendererSettings': (0, [], []),
    'RenderInChromeFrameList': ([ 'google.com' ], [], []),
    'RenderInHostList': ([ 'google.com' ], [], []),
    'ChromeFrameContentTypes': ([ 'text/xml' ], [], []),
    'GCFUserDataDir': ('${user_name}/test-frame', [], []),
  }
