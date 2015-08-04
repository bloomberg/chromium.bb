// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_PREF_NAMES_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_PREF_NAMES_H_

#include "base/logging.h"

namespace prefs {

extern const char kBlockThirdPartyCookies[];
extern const char kContentSettingsVersion[];
extern const char kContentSettingsWindowLastTabIndex[];

extern const char kDefaultCookiesSetting[];
extern const char kDefaultImagesSetting[];
extern const char kDefaultJavaScriptSetting[];
extern const char kDefaultPluginsSetting[];
extern const char kDefaultPopupsSetting[];
extern const char kDefaultGeolocationSetting[];
extern const char kDefaultNotificationsSetting[];
extern const char kDefaultAutoSelectCertificateSetting[];
extern const char kDefaultFullScreenSetting[];
extern const char kDefaultMouseLockSetting[];
extern const char kDefaultMixedScriptSetting[];
extern const char kDefaultMediaStreamSetting[];
extern const char kDefaultMediaStreamMicSetting[];
extern const char kDefaultMediaStreamCameraSetting[];
extern const char kDefaultProtocolHandlersSetting[];
extern const char kDefaultPpapiBrokerSetting[];
extern const char kDefaultAutomaticDownloadsSetting[];
extern const char kDefaultMidiSysexSetting[];
extern const char kDefaultPushMessagingSetting[];
extern const char kDefaultSSLCertDecisionsSetting[];
#if defined(OS_WIN)
extern const char kDefaultMetroSwitchToDesktopSetting[];
#elif defined(OS_ANDROID) || defined(OS_CHROMEOS)
extern const char kDefaultProtectedMediaIdentifierSetting[];
#endif
extern const char kDefaultAppBannerSetting[];
extern const char kDefaultSiteEngagementSetting[];
extern const char kDefaultDurableStorageSetting[];

extern const char kMigratedDefaultMediaStreamSetting[];

// Preferences storing the default values for individual content settings.
extern const char kContentSettingsCookiesPatternPairs[];
extern const char kContentSettingsImagesPatternPairs[];
extern const char kContentSettingsJavaScriptPatternPairs[];
extern const char kContentSettingsPluginsPatternPairs[];
extern const char kContentSettingsPopupsPatternPairs[];
extern const char kContentSettingsGeolocationPatternPairs[];
extern const char kContentSettingsNotificationsPatternPairs[];
extern const char kContentSettingsAutoSelectCertificatePatternPairs[];
extern const char kContentSettingsFullScreenPatternPairs[];
extern const char kContentSettingsMouseLockPatternPairs[];
extern const char kContentSettingsMixedScriptPatternPairs[];
extern const char kContentSettingsMediaStreamPatternPairs[];
extern const char kContentSettingsMediaStreamMicPatternPairs[];
extern const char kContentSettingsMediaStreamCameraPatternPairs[];
extern const char kContentSettingsProtocolHandlersPatternPairs[];
extern const char kContentSettingsPpapiBrokerPatternPairs[];
extern const char kContentSettingsAutomaticDownloadsPatternPairs[];
extern const char kContentSettingsMidiSysexPatternPairs[];
extern const char kContentSettingsPushMessagingPatternPairs[];
extern const char kContentSettingsSSLCertDecisionsPatternPairs[];
#if defined(OS_WIN)
extern const char kContentSettingsMetroSwitchToDesktopPatternPairs[];
#elif defined(OS_ANDROID) || defined(OS_CHROMEOS)
extern const char kContentSettingsProtectedMediaIdentifierPatternPairs[];
#endif
extern const char kContentSettingsAppBannerPatternPairs[];
extern const char kContentSettingsSiteEngagementPatternPairs[];
extern const char kContentSettingsDurableStoragePatternPairs[];

extern const char kManagedDefaultCookiesSetting[];
extern const char kManagedDefaultImagesSetting[];
extern const char kManagedDefaultJavaScriptSetting[];
extern const char kManagedDefaultPluginsSetting[];
extern const char kManagedDefaultPopupsSetting[];
extern const char kManagedDefaultGeolocationSetting[];
extern const char kManagedDefaultNotificationsSetting[];
extern const char kManagedDefaultMediaStreamSetting[];

extern const char kManagedCookiesAllowedForUrls[];
extern const char kManagedCookiesBlockedForUrls[];
extern const char kManagedCookiesSessionOnlyForUrls[];
extern const char kManagedImagesAllowedForUrls[];
extern const char kManagedImagesBlockedForUrls[];
extern const char kManagedJavaScriptAllowedForUrls[];
extern const char kManagedJavaScriptBlockedForUrls[];
extern const char kManagedPluginsAllowedForUrls[];
extern const char kManagedPluginsBlockedForUrls[];
extern const char kManagedPopupsAllowedForUrls[];
extern const char kManagedPopupsBlockedForUrls[];
extern const char kManagedNotificationsAllowedForUrls[];
extern const char kManagedNotificationsBlockedForUrls[];
extern const char kManagedAutoSelectCertificateForUrls[];

}  // namespace prefs

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_COMMON_PREF_NAMES_H_
