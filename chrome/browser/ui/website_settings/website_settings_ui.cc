// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/website_settings_ui.h"

#include "grit/generated_resources.h"

namespace {
const int kInvalidRessourceID = -1;
}

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

// static
int WebsiteSettingsUI::PermissionTypeToUIStringID(ContentSettingsType type) {
  switch (type) {
    case CONTENT_SETTINGS_TYPE_POPUPS:
      return IDS_WEBSITE_SETTINGS_TYPE_POPUPS;
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return IDS_WEBSITE_SETTINGS_TYPE_PLUGINS;
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return IDS_WEBSITE_SETTINGS_TYPE_LOCATION;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      return IDS_WEBSITE_SETTINGS_TYPE_NOTIFICATIONS;
    default:
      NOTREACHED();
      return kInvalidRessourceID;
  }
}

// static
int WebsiteSettingsUI::PermissionValueToUIStringID(ContentSetting value) {
  switch (value) {
    case CONTENT_SETTING_ALLOW:
      return IDS_WEBSITE_SETTINGS_PERMISSION_ALLOW;
    case CONTENT_SETTING_BLOCK:
      return IDS_WEBSITE_SETTINGS_PERMISSION_BLOCK;
    case CONTENT_SETTING_ASK:
      return IDS_WEBSITE_SETTINGS_PERMISSION_ASK;
    default:
      NOTREACHED();
      return kInvalidRessourceID;
  }
}
