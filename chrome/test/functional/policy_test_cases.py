#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This data is in a separate file so that src/chrome/app/policy/PRESUBMIT.py
# can load it too without having to load pyautolib.

# DO NOT import pyauto from here! This file is required to run a presubmit
# scripts that will always fail if pyautolib isn't available, which is common.


class PolicyPrefsTestCases(object):
  """A list of test cases for policy_prefs_ui.py."""

  [BROWSER, SEARCH_ENGINES, PASSWORDS, AUTOFILL, CONTENT, HOMEPAGE, LANGUAGES,
   ACCOUNTS] = range(8)

  # Each policy has an entry with a tuple (Pref, Value, Pages, OS)
  #
  # |Pref| is the preference key for simple user policies that map directly to a
  #        preference (Refer to
  #        chrome/browser/policy/configuration_policy_handler_list.cc).
  #        Otherwise, set as None.
  #
  # |Value| is the value the policy should have when it is enforced.
  #
  # |Pages| is a list with integer indices into |settings_pages|, and lists all
  #         the pages that should display the managed-banner.
  #         Leave empty if this policy doesn't display anything visible in
  #         the settings.
  #
  # |OS| is a list of platforms where this policy should be tested. Valid
  #      platforms are 'win', 'mac', 'linux', and 'chromeos'. The list can be
  #      empty to skip the policy or set to OS_ALL if applicable to all
  #      platforms.
  #
  # ChromeOS device policies are also listed but are currently not tested by
  # policy_prefs_ui.py.
  [INDEX_PREF, INDEX_VALUE, INDEX_PAGES, INDEX_OS] = range(4)
  OS_ALL = ['win', 'mac', 'linux', 'chromeos']
  policies = {
    'HomepageLocation':
        ('kHomePage', 'http://chromium.org', [HOMEPAGE], OS_ALL),
    'HomepageIsNewTabPage':
        ('kHomePageIsNewTabPage', True, [HOMEPAGE], OS_ALL),
    # TODO(joaodasilva): Couldn't verify on linux.
    'DefaultBrowserSettingEnabled':
        ('kDefaultBrowserSettingEnabled', False, [], ['win', 'mac', 'linux']),
    # TODO(joaodasilva): Test this on windows.
    'ApplicationLocaleValue': ('kApplicationLocale', '', [], ['win']),
    'AlternateErrorPagesEnabled':
        ('kAlternateErrorPagesEnabled', False, [BROWSER], OS_ALL),
    'SearchSuggestEnabled':
        ('kSearchSuggestEnabled', False, [BROWSER], OS_ALL),
    'DnsPrefetchingEnabled':
        ('kNetworkPredictionEnabled', False, [BROWSER], OS_ALL),
    'DisableSpdy': ('kDisableSpdy', True, [], OS_ALL),
    'DisabledSchemes': ('kDisabledSchemes', ['file'], [], OS_ALL),
    'JavascriptEnabled': (None, False, [CONTENT], OS_ALL),
    'IncognitoEnabled': (None, False, [], OS_ALL),
    'IncognitoModeAvailability': (None, 1, [], OS_ALL),
    'SavingBrowserHistoryDisabled':
        ('kSavingBrowserHistoryDisabled', True, [], OS_ALL),
    'RemoteAccessClientFirewallTraversal': (None, True, [], OS_ALL),
    # TODO(frankf): Enable on all OS after crbug.com/121066 is fixed.
    'RemoteAccessHostFirewallTraversal':
        ('kRemoteAccessHostFirewallTraversal', True, [], []),
    'PrintingEnabled': ('kPrintingEnabled', False, [], OS_ALL),
    # Note: supported_on is empty for this policy.
    'CloudPrintProxyEnabled': ('kCloudPrintProxyEnabled', True, [], []),
    'CloudPrintSubmitEnabled':
        ('kCloudPrintSubmitEnabled', False, [], ['win', 'mac', 'linux']),
    'SafeBrowsingEnabled': ('kSafeBrowsingEnabled', False, [BROWSER], OS_ALL),
    # TODO(joaodasilva): This is only in place on official builds, but the
    # SetUserCloudPolicy call is a nop on official builds. Should be BROWSER.
    'MetricsReportingEnabled':
        ('kMetricsReportingEnabled', False, [], ['win', 'mac', 'linux']),
    'PasswordManagerEnabled':
        ('kPasswordManagerEnabled', False, [BROWSER], OS_ALL),
    # TODO(joaodasilva): Should be PASSWORDS too. http://crbug.com/97749
    'PasswordManagerAllowShowPasswords':
        ('kPasswordManagerAllowShowPasswords', False, [BROWSER], OS_ALL),
    'AutoFillEnabled': ('kAutofillEnabled', False, [BROWSER], OS_ALL),
    'DisabledPlugins': ('kPluginsDisabledPlugins', ['Flash'], [], OS_ALL),
    'EnabledPlugins': ('kPluginsEnabledPlugins', ['Flash'], [], OS_ALL),
    'DisabledPluginsExceptions':
        ('kPluginsDisabledPluginsExceptions', ['Flash'], [], OS_ALL),
    'DisablePluginFinder': ('kDisablePluginFinder', True, [], OS_ALL),
    # TODO(joaodasilva): Should be PERSONAL. http://crbug.com/97749
    'SyncDisabled': (None, True, [], OS_ALL),
    'UserDataDir':
        (None, '${users}/${user_name}/chrome-test', [], ['win', 'mac']),
    'DiskCacheDir':
        ('kDiskCacheDir', '${user_home}/test-cache', [],
         ['win', 'mac', 'linux']),
    'DiskCacheSize': ('kDiskCacheSize', 100, [], ['win', 'mac', 'linux']),
    'MediaCacheSize': ('kMediaCacheSize', 200, [], ['win', 'mac', 'linux']),
    'DownloadDirectory': (None, '${user_home}/test-downloads', [BROWSER],
                          ['win', 'mac', 'linux']),
    'ClearSiteDataOnExit': ('kClearSiteDataOnExit', True, [CONTENT], OS_ALL),
    # TODO(joaodasilva): Should be BROWSER. http://crbug.com/97749
    'ProxyMode': (None, 'direct', [], ['win', 'mac', 'linux']),
    # TODO(joaodasilva): Should be BROWSER. http://crbug.com/97749
    'ProxyServerMode': (None, 0, [], ['win', 'mac', 'linux']),
    'ProxyServer':
        (None, 'http://localhost:8080', [], ['win', 'mac', 'linux']),
    'ProxyPacUrl':
        (None, 'http://localhost:8080/proxy.pac', [],
         ['win', 'mac', 'linux']),
    'ProxyBypassList': (None, 'localhost', [], ['win', 'mac', 'linux']),
    # Note: this policy is only used internally for now.
    'ProxySettings': (None, {}, [], []),
    'EnableOriginBoundCerts':
        ('kEnableOriginBoundCerts', False, [], ['win', 'mac', 'linux']),
    'DisableSSLRecordSplitting':
        ('kDisableSSLRecordSplitting', False, [], OS_ALL),
    'EnableOnlineRevocationChecks':
        ('kCertRevocationCheckingEnabled', False, [], OS_ALL),
    'AuthSchemes': ('kAuthSchemes', 'AuthSchemes', [], OS_ALL),
    'DisableAuthNegotiateCnameLookup':
        ('kDisableAuthNegotiateCnameLookup', True, [], OS_ALL),
    'EnableAuthNegotiatePort':
        ('kEnableAuthNegotiatePort', False, [], OS_ALL),
    'AuthServerWhitelist': ('kAuthServerWhitelist', 'localhost', [], OS_ALL),
    'AuthNegotiateDelegateWhitelist':
        ('kAuthNegotiateDelegateWhitelist', 'localhost', [], OS_ALL),
    'GSSAPILibraryName':
        ('kGSSAPILibraryName', 'libwhatever.so', [], ['mac', 'linux']),
    'AllowCrossOriginAuthPrompt':
        ('kAllowCrossOriginAuthPrompt', False, [], ['win', 'mac', 'linux']),
    'ExtensionInstallBlacklist':
        ('kExtensionInstallDenyList', ['*'], [], OS_ALL),
    'ExtensionInstallWhitelist':
        ('kExtensionInstallAllowList', ['lcncmkcnkcdbbanbjakcencbaoegdjlp'],
         [], OS_ALL),
    'ExtensionInstallForcelist':
      ('kExtensionInstallForceList', ['lcncmkcnkcdbbanbjakcencbaoegdjlp;' +
       'https://clients2.google.com/service/update2/crx'], [], OS_ALL),
    'ShowHomeButton': ('kShowHomeButton', True, [BROWSER], OS_ALL),
    'DeveloperToolsDisabled': ('kDevToolsDisabled', True, [], OS_ALL),
    'RestoreOnStartup': (None, 5, [BROWSER], OS_ALL),
    # TODO(joaodasilva): Should be BROWSER. http://crbug.com/97749
    'RestoreOnStartupURLs':
        ('kURLsToRestoreOnStartup', ['chromium.org'], [], OS_ALL),
    # TODO(joaodasilva): The banner is out of place. http://crbug.com/77791
    'BlockThirdPartyCookies':
        ('kBlockThirdPartyCookies', True, [CONTENT], OS_ALL),
    # TODO(joaodasilva): Should be BROWSER. http://crbug.com/97749
    'DefaultSearchProviderEnabled': (None, False, [], OS_ALL),
    'DefaultSearchProviderName': (None, 'google.com', [], OS_ALL),
    'DefaultSearchProviderKeyword': (None, 'google', [], OS_ALL),
    # TODO(joaodasilva): Should be BROWSER. http://crbug.com/97749
    'DefaultSearchProviderSearchURL':
        (None, 'http://www.google.com/?q={searchTerms}', [], OS_ALL),
    'DefaultSearchProviderSuggestURL':
        (None, 'http://www.google.com/suggest?q={searchTerms}', [], OS_ALL),
    'DefaultSearchProviderInstantURL':
        (None, 'http://www.google.com/instant?q={searchTerms}', [], OS_ALL),
    'DefaultSearchProviderIconURL':
        (None, 'http://www.google.com/favicon.ico', [], OS_ALL),
    'DefaultSearchProviderEncodings': (None, ['UTF-8'], [], OS_ALL),
    'DefaultCookiesSetting':
        ('kManagedDefaultCookiesSetting', 2, [CONTENT], OS_ALL),
    'DefaultImagesSetting':
        ('kManagedDefaultImagesSetting', 2, [CONTENT], OS_ALL),
    'DefaultJavaScriptSetting': (None, 2, [CONTENT], OS_ALL),
    'DefaultPluginsSetting':
        ('kManagedDefaultPluginsSetting', 2, [CONTENT], OS_ALL),
    'DefaultPopupsSetting':
        ('kManagedDefaultPopupsSetting', 2, [CONTENT], OS_ALL),
    'DefaultNotificationsSetting':
        ('kManagedDefaultNotificationsSetting', 2, [CONTENT], OS_ALL),
    'DefaultGeolocationSetting':
        ('kManagedDefaultGeolocationSetting', 2, [CONTENT], OS_ALL),
    'AutoSelectCertificateForUrls':
        ('kManagedAutoSelectCertificateForUrls',
         ['{\'pattern\':\'https://example.com\',' +
          '\'filter\':{\'ISSUER\':{\'CN\': \'issuer-name\'}}}'], [], OS_ALL),
    'CookiesAllowedForUrls':
        ('kManagedCookiesAllowedForUrls', ['[*.]google.com'], [], OS_ALL),
    'CookiesBlockedForUrls':
        ('kManagedCookiesBlockedForUrls', ['[*.]google.com'], [], OS_ALL),
    'CookiesSessionOnlyForUrls':
        ('kManagedCookiesSessionOnlyForUrls', ['[*.]google.com'], [], OS_ALL),
    'ImagesAllowedForUrls':
        ('kManagedImagesAllowedForUrls', ['[*.]google.com'], [], OS_ALL),
    'ImagesBlockedForUrls':
        ('kManagedImagesBlockedForUrls', ['[*.]google.com'], [], OS_ALL),
    'JavaScriptAllowedForUrls':
        ('kManagedJavaScriptAllowedForUrls', ['[*.]google.com'], [], OS_ALL),
    'JavaScriptBlockedForUrls':
        ('kManagedJavaScriptBlockedForUrls', ['[*.]google.com'], [], OS_ALL),
    'PluginsAllowedForUrls':
        ('kManagedPluginsAllowedForUrls', ['[*.]google.com'], [], OS_ALL),
    'PluginsBlockedForUrls':
        ('kManagedPluginsBlockedForUrls', ['[*.]google.com'], [], OS_ALL),
    'PopupsAllowedForUrls':
        ('kManagedPopupsAllowedForUrls', ['[*.]google.com'], [], OS_ALL),
    'PopupsBlockedForUrls':
        ('kManagedPopupsBlockedForUrls', ['[*.]google.com'], [], OS_ALL),
    'NotificationsAllowedForUrls':
        ('kManagedNotificationsAllowedForUrls', ['[*.]google.com'], [],
         OS_ALL),
    'NotificationsBlockedForUrls':
        ('kManagedNotificationsBlockedForUrls', ['[*.]google.com'], [],
         OS_ALL),
    'Disable3DAPIs': ('kDisable3DAPIs', True, [], OS_ALL),
    'InstantEnabled': ('kInstantEnabled', False, [BROWSER], OS_ALL),
    'TranslateEnabled': ('kEnableTranslate', False, [BROWSER], OS_ALL),
    'AllowOutdatedPlugins': ('kPluginsAllowOutdated', False, [], OS_ALL),
    'AlwaysAuthorizePlugins': ('kPluginsAlwaysAuthorize', True, [], OS_ALL),
    'BookmarkBarEnabled': ('kShowBookmarkBar', False, [BROWSER], OS_ALL),
    'EditBookmarksEnabled': ('kEditBookmarksEnabled', False, [], OS_ALL),
    'AllowFileSelectionDialogs':
        ('kAllowFileSelectionDialogs', False, [BROWSER],
         ['win', 'mac', 'linux']),
    'ImportBookmarks':
        ('kImportBookmarks', False, [], ['win', 'mac', 'linux']),
    'ImportHistory':
        ('kImportHistory', False, [], ['win', 'mac', 'linux']),
    'ImportHomepage':
        ('kImportHomepage', False, [], ['win', 'mac', 'linux']),
    'ImportSearchEngine':
        ('kImportSearchEngine', False, [], ['win', 'mac', 'linux']),
    'ImportSavedPasswords':
        ('kImportSavedPasswords', False, [], ['win', 'mac', 'linux']),
    'MaxConnectionsPerProxy': ('kMaxConnectionsPerProxy', 32, [], OS_ALL),
    'HideWebStorePromo': ('kNtpHideWebStorePromo', True, [], OS_ALL),
    'URLBlacklist': ('kUrlBlacklist', ['google.com'], [], OS_ALL),
    'URLWhitelist': ('kUrlWhitelist', ['google.com'], [], OS_ALL),
    'EnterpriseWebStoreURL': ('kEnterpriseWebStoreURL', '', [], OS_ALL),
    'EnterpriseWebStoreName': ('kEnterpriseWebStoreName', '', [], OS_ALL),
    'EnableMemoryInfo': ('kEnableMemoryInfo', True, [], OS_ALL),
    'DisablePrintPreview':
        ('kPrintPreviewDisabled', True, [], ['win', 'mac', 'linux']),
    'BackgroundModeEnabled':
        ('kBackgroundModeEnabled', True, [BROWSER], ['win', 'linux']),

    # ChromeOS-only policies:
    # TODO(frankf): Add prefs for these after crosbug.com/28756 is fixed.
    'ChromeOsLockOnIdleSuspend':
        (None, True, [BROWSER], ['chromeos']),
    'PolicyRefreshRate':
        (None, 300000, [], ['chromeos']),
    'OpenNetworkConfiguration': (None, '', [], ['chromeos']),
    'GDataDisabled': (None, True, [], ['chromeos']),
    'GDataDisabledOverCellular':
        (None, True, [], ['chromeos']),
    'PinnedLauncherApps': (None, [], [], ['chromeos']),

    # ChromeOS Device policies:
    'DevicePolicyRefreshRate': (None, 300000, [], ['chromeos']),
    'ChromeOsReleaseChannel': (None, 'stable-channel', [], ['chromeos']),
    'ChromeOsReleaseChannelDelegated': (None, False, [], ['chromeos']),
    'DeviceOpenNetworkConfiguration': (None, '', [], ['chromeos']),
    'ReportDeviceVersionInfo': (None, True, [], ['chromeos']),
    'ReportDeviceActivityTimes': (None, True, [], ['chromeos']),
    'ReportDeviceBootMode': (None, True, [], ['chromeos']),
    'DeviceAllowNewUsers': (None, True, [], ['chromeos']),
    'DeviceUserWhitelist': (None, [], [], ['chromeos']),
    'DeviceGuestModeEnabled': (None, True, [], ['chromeos']),
    'DeviceShowUserNamesOnSignin': (None, True, [], ['chromeos']),
    'DeviceDataRoamingEnabled': (None, True, [], ['chromeos']),
    'DeviceMetricsReportingEnabled': (None, True, [], ['chromeos']),
    'DeviceEphemeralUsersEnabled': (None, True, [], ['chromeos']),
    'DeviceIdleLogoutTimeout': (None, 60000, [], ['chromeos']),
    'DeviceIdleLogoutWarningDuration': (None, 15000, [], ['chromeos']),
    'DeviceLoginScreenSaverId':
        (None, 'lcncmkcnkcdbbanbjakcencbaoegdjlp', [], ['chromeos']),
    'DeviceLoginScreenSaverTimeout': (None, 30000, [], ['chromeos']),
    'DeviceStartUpUrls': (None, ['http://google.com'], [], ['chromeos']),
    'DeviceAppPack': (None, [], [], ['chromeos']),
    'DeviceAutoUpdateDisabled': (None, True, [], ['chromeos']),
    'DeviceTargetVersionPrefix': (None, '1412.', [], ['chromeos']),
    'ReportDeviceLocation': (None, False, [], ['chromeos']),

    # Chrome Frame policies:
    'ChromeFrameRendererSettings': (None, 0, [], []),
    'RenderInChromeFrameList': (None, ['google.com'], [], []),
    'RenderInHostList': (None, ['google.com'], [], []),
    'ChromeFrameContentTypes': (None, ['text/xml'], [], []),
    'GCFUserDataDir': (None, '${user_name}/test-frame', [], []),
    'AdditionalLaunchParameters': (None, '--enable-media-stream', [], []),
  }
