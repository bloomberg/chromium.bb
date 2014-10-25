// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/pref_names.h"

namespace prefs {

// Version of the pattern format used to define content settings.
const char kContentSettingsVersion[] = "profile.content_settings.pref_version";

// Patterns for mapping origins to origin related settings. Default settings
// will be applied to origins that don't match any of the patterns. The pattern
// format used is defined by kContentSettingsVersion.
const char kContentSettingsPatternPairs[] =
    "profile.content_settings.pattern_pairs";

// Integer that specifies the index of the tab the user was on when they
// last visited the content settings window.
const char kContentSettingsWindowLastTabIndex[] =
    "content_settings_window.last_tab_index";

// Dictionary of content settings applied to all hosts by default.
const char kDefaultContentSettings[] = "profile.default_content_settings";

// Dictionary of content settings that can globally disallow all hosts by
// default. If a value is set, it means the setting is globally disallowed.
// If a value is not set, it means the setting is allowed.
const char kOverrideContentSettings[] = "profile.override_content_settings";

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
