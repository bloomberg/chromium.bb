// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/pref_names.h"

namespace prefs {

// Boolean that is true if we should unconditionally block third-party cookies,
// regardless of other content settings.
const char kBlockThirdPartyCookies[] = "profile.block_third_party_cookies";

// Version of the pattern format used to define content settings.
const char kContentSettingsVersion[] = "profile.content_settings.pref_version";

// Integer that specifies the index of the tab the user was on when they
// last visited the content settings window.
const char kContentSettingsWindowLastTabIndex[] =
    "content_settings_window.last_tab_index";

// Preferences storing the default values for individual content settings.
const char kDefaultCookiesSetting[] =
    "profile.default_content_setting_values.cookies";
const char kDefaultImagesSetting[] =
    "profile.default_content_setting_values.images";
const char kDefaultJavaScriptSetting[] =
    "profile.default_content_setting_values.javascript";
const char kDefaultPluginsSetting[] =
    "profile.default_content_setting_values.plugins";
const char kDefaultPopupsSetting[] =
    "profile.default_content_setting_values.popups";
const char kDefaultGeolocationSetting[] =
    "profile.default_content_setting_values.geolocation";
const char kDefaultNotificationsSetting[] =
    "profile.default_content_setting_values.notifications";
const char kDefaultAutoSelectCertificateSetting[] =
    "profile.default_content_setting_values.auto_select_certificate";
const char kDefaultFullScreenSetting[] =
    "profile.default_content_setting_values.fullscreen";
const char kDefaultMouseLockSetting[] =
    "profile.default_content_setting_values.mouselock";
const char kDefaultMixedScriptSetting[] =
    "profile.default_content_setting_values.mixed_script";
const char kDefaultMediaStreamSetting[] =
    "profile.default_content_setting_values.media_stream";
const char kDefaultMediaStreamMicSetting[] =
    "profile.default_content_setting_values.media_stream_mic";
const char kDefaultMediaStreamCameraSetting[] =
    "profile.default_content_setting_values.media_stream_camera";
const char kDefaultProtocolHandlersSetting[] =
    "profile.default_content_setting_values.protocol_handlers";
const char kDefaultPpapiBrokerSetting[] =
    "profile.default_content_setting_values.ppapi_broker";
const char kDefaultAutomaticDownloadsSetting[] =
    "profile.default_content_setting_values.automatic_downloads";
const char kDefaultMidiSysexSetting[] =
    "profile.default_content_setting_values.midi_sysex";
const char kDefaultPushMessagingSetting[] =
    "profile.default_content_setting_values.push_messaging";
const char kDefaultSSLCertDecisionsSetting[] =
    "profile.default_content_setting_values.ssl_cert_decisions";
#if defined(OS_WIN)
const char kDefaultMetroSwitchToDesktopSetting[] =
    "profile.default_content_setting_values.metro_switch_to_desktop";
#elif defined(OS_ANDROID) || defined(OS_CHROMEOS)
const char kDefaultProtectedMediaIdentifierSetting[] =
    "profile.default_content_setting_values.protected_media_identifier";
#endif
const char kDefaultAppBannerSetting[] =
    "profile.default_content_setting_values.app_banner";
const char kDefaultSiteEngagementSetting[] =
    "profile.default_content_setting_values.site_engagement";
const char kDefaultDurableStorageSetting[] =
    "profile.default_content_setting_values.durable_storage";

// Boolean indicating whether the media stream default setting had been
// migrated into two separate microphone and camera settings.
const char kMigratedDefaultMediaStreamSetting[] =
    "profile.migrated_default_media_stream_content_settings";

// Preferences storing the content settings exceptions.
const char kContentSettingsCookiesPatternPairs[] =
    "profile.content_settings.exceptions.cookies";
const char kContentSettingsImagesPatternPairs[] =
    "profile.content_settings.exceptions.images";
const char kContentSettingsJavaScriptPatternPairs[] =
    "profile.content_settings.exceptions.javascript";
const char kContentSettingsPluginsPatternPairs[] =
    "profile.content_settings.exceptions.plugins";
const char kContentSettingsPopupsPatternPairs[] =
    "profile.content_settings.exceptions.popups";
const char kContentSettingsGeolocationPatternPairs[] =
    "profile.content_settings.exceptions.geolocation";
const char kContentSettingsNotificationsPatternPairs[] =
    "profile.content_settings.exceptions.notifications";
const char kContentSettingsAutoSelectCertificatePatternPairs[] =
    "profile.content_settings.exceptions.auto_select_certificate";
const char kContentSettingsFullScreenPatternPairs[] =
    "profile.content_settings.exceptions.fullscreen";
const char kContentSettingsMouseLockPatternPairs[] =
    "profile.content_settings.exceptions.mouselock";
const char kContentSettingsMixedScriptPatternPairs[] =
    "profile.content_settings.exceptions.mixed_script";
const char kContentSettingsMediaStreamPatternPairs[] =
    "profile.content_settings.exceptions.media_stream";
const char kContentSettingsMediaStreamMicPatternPairs[] =
    "profile.content_settings.exceptions.media_stream_mic";
const char kContentSettingsMediaStreamCameraPatternPairs[] =
    "profile.content_settings.exceptions.media_stream_camera";
const char kContentSettingsProtocolHandlersPatternPairs[] =
    "profile.content_settings.exceptions.protocol_handlers";
const char kContentSettingsPpapiBrokerPatternPairs[] =
    "profile.content_settings.exceptions.ppapi_broker";
const char kContentSettingsAutomaticDownloadsPatternPairs[] =
    "profile.content_settings.exceptions.automatic_downloads";
const char kContentSettingsMidiSysexPatternPairs[] =
    "profile.content_settings.exceptions.midi_sysex";
const char kContentSettingsPushMessagingPatternPairs[] =
    "profile.content_settings.exceptions.push_messaging";
const char kContentSettingsSSLCertDecisionsPatternPairs[] =
    "profile.content_settings.exceptions.ssl_cert_decisions";
#if defined(OS_WIN)
const char kContentSettingsMetroSwitchToDesktopPatternPairs[] =
    "profile.content_settings.exceptions.metro_switch_to_desktop";
#elif defined(OS_ANDROID) || defined(OS_CHROMEOS)
const char kContentSettingsProtectedMediaIdentifierPatternPairs[] =
    "profile.content_settings.exceptions.protected_media_identifier";
#endif
const char kContentSettingsAppBannerPatternPairs[] =
    "profile.content_settings.exceptions.app_banner";
const char kContentSettingsSiteEngagementPatternPairs[] =
    "profile.content_settings.exceptions.site_engagement";
const char kContentSettingsDurableStoragePatternPairs[] =
    "profile.content_settings.exceptions.durable_storage";

// Preferences that are exclusively used to store managed values for default
// content settings.
const char kManagedDefaultCookiesSetting[] =
    "profile.managed_default_content_settings.cookies";
const char kManagedDefaultImagesSetting[] =
    "profile.managed_default_content_settings.images";
const char kManagedDefaultJavaScriptSetting[] =
    "profile.managed_default_content_settings.javascript";
const char kManagedDefaultPluginsSetting[] =
    "profile.managed_default_content_settings.plugins";
const char kManagedDefaultPopupsSetting[] =
    "profile.managed_default_content_settings.popups";
const char kManagedDefaultGeolocationSetting[] =
    "profile.managed_default_content_settings.geolocation";
const char kManagedDefaultNotificationsSetting[] =
    "profile.managed_default_content_settings.notifications";
const char kManagedDefaultMediaStreamSetting[] =
    "profile.managed_default_content_settings.media_stream";

// Preferences that are exclusively used to store managed
// content settings patterns.
const char kManagedCookiesAllowedForUrls[] =
    "profile.managed_cookies_allowed_for_urls";
const char kManagedCookiesBlockedForUrls[] =
    "profile.managed_cookies_blocked_for_urls";
const char kManagedCookiesSessionOnlyForUrls[] =
    "profile.managed_cookies_sessiononly_for_urls";
const char kManagedImagesAllowedForUrls[] =
    "profile.managed_images_allowed_for_urls";
const char kManagedImagesBlockedForUrls[] =
    "profile.managed_images_blocked_for_urls";
const char kManagedJavaScriptAllowedForUrls[] =
    "profile.managed_javascript_allowed_for_urls";
const char kManagedJavaScriptBlockedForUrls[] =
    "profile.managed_javascript_blocked_for_urls";
const char kManagedPluginsAllowedForUrls[] =
    "profile.managed_plugins_allowed_for_urls";
const char kManagedPluginsBlockedForUrls[] =
    "profile.managed_plugins_blocked_for_urls";
const char kManagedPopupsAllowedForUrls[] =
    "profile.managed_popups_allowed_for_urls";
const char kManagedPopupsBlockedForUrls[] =
    "profile.managed_popups_blocked_for_urls";
const char kManagedNotificationsAllowedForUrls[] =
    "profile.managed_notifications_allowed_for_urls";
const char kManagedNotificationsBlockedForUrls[] =
    "profile.managed_notifications_blocked_for_urls";
const char kManagedAutoSelectCertificateForUrls[] =
    "profile.managed_auto_select_certificate_for_urls";

}  // namespace prefs
