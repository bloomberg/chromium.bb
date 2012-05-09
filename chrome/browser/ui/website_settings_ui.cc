// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings_ui.h"

WebsiteSettingsUI::CookieInfo::CookieInfo()
    : allowed(-1), blocked(-1) {
}

WebsiteSettingsUI::PermissionInfo::PermissionInfo()
    : type(CONTENT_SETTINGS_TYPE_DEFAULT),
      setting(CONTENT_SETTING_DEFAULT),
      default_setting(CONTENT_SETTING_DEFAULT) {
}

WebsiteSettingsUI::IdentityInfo::IdentityInfo()
    : identity_status(WebsiteSettings::SITE_IDENTITY_STATUS_UNKNOWN),
      cert_id(0),
      connection_status(WebsiteSettings::SITE_CONNECTION_STATUS_UNKNOWN) {
}

WebsiteSettingsUI::~WebsiteSettingsUI() {
}
