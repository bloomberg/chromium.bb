// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/website_settings_ui.h"

#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

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
    case CONTENT_SETTINGS_TYPE_IMAGES:
     return IDS_WEBSITE_SETTINGS_TYPE_IMAGES;
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
     return IDS_WEBSITE_SETTINGS_TYPE_JAVASCRIPT;
    case CONTENT_SETTINGS_TYPE_POPUPS:
      return IDS_WEBSITE_SETTINGS_TYPE_POPUPS;
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return IDS_WEBSITE_SETTINGS_TYPE_PLUGINS;
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return IDS_WEBSITE_SETTINGS_TYPE_LOCATION;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      return IDS_WEBSITE_SETTINGS_TYPE_NOTIFICATIONS;
    case CONTENT_SETTINGS_TYPE_FULLSCREEN:
      return IDS_WEBSITE_SETTINGS_TYPE_FULLSCREEN;
    case CONTENT_SETTINGS_TYPE_MOUSELOCK:
      return IDS_WEBSITE_SETTINGS_TYPE_MOUSELOCK;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
      return IDS_WEBSITE_SETTINGS_TYPE_MEDIASTREAM;
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

// static
int WebsiteSettingsUI::PermissionActionUIStringID(ContentSetting value) {
  switch (value) {
    case CONTENT_SETTING_ALLOW:
      return IDS_WEBSITE_SETTINGS_PERMISSION_ACTION_ALLOWED;
    case CONTENT_SETTING_BLOCK:
      return IDS_WEBSITE_SETTINGS_PERMISSION_ACTION_BLOCKED;
    case CONTENT_SETTING_ASK:
      return IDS_WEBSITE_SETTINGS_PERMISSION_ACTION_ASK;
    default:
      NOTREACHED();
      return kInvalidRessourceID;
  }
}

// static
const gfx::Image& WebsiteSettingsUI::GetPermissionIcon(
    ContentSettingsType type,
    ContentSetting setting) {
  bool use_blocked = (setting == CONTENT_SETTING_BLOCK);
  int resource_id = IDR_INFO;
  switch (type) {
    case CONTENT_SETTINGS_TYPE_IMAGES:
      resource_id = use_blocked ? IDR_BLOCKED_IMAGES
                                : IDR_INFO;
      break;
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
      resource_id = use_blocked ? IDR_BLOCKED_JAVASCRIPT
                                : IDR_INFO;
      break;
    case CONTENT_SETTINGS_TYPE_COOKIES:
      resource_id = use_blocked ? IDR_BLOCKED_COOKIES
                                : IDR_COOKIE_ICON;
      break;
    case CONTENT_SETTINGS_TYPE_POPUPS:
      resource_id = use_blocked ? IDR_BLOCKED_POPUPS
                                : IDR_INFO;
      break;
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      resource_id = use_blocked ? IDR_BLOCKED_PLUGINS
                                : IDR_EXTENSIONS_FAVICON;
      break;
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      resource_id = use_blocked ? IDR_GEOLOCATION_DENIED_LOCATIONBAR_ICON
                                : IDR_GEOLOCATION_ALLOWED_LOCATIONBAR_ICON;
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
    case CONTENT_SETTINGS_TYPE_FULLSCREEN:
    case CONTENT_SETTINGS_TYPE_MOUSELOCK:
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
      resource_id = IDR_INFO;
      break;
    default:
      NOTREACHED();
      break;
  }
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(resource_id);
}

// static
const gfx::Image& WebsiteSettingsUI::GetIdentityIcon(
    WebsiteSettings::SiteIdentityStatus status) {
  int resource_id = IDR_PAGEINFO_INFO;
  switch (status) {
    case WebsiteSettings::SITE_IDENTITY_STATUS_UNKNOWN:
      break;
    case WebsiteSettings::SITE_IDENTITY_STATUS_CERT:
    case WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT:
    case WebsiteSettings::SITE_IDENTITY_STATUS_DNSSEC_CERT:
      resource_id = IDR_PAGEINFO_GOOD;
      break;
    case WebsiteSettings::SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN:
      resource_id = IDR_PAGEINFO_WARNING_MINOR;
      break;
    case WebsiteSettings::SITE_IDENTITY_STATUS_NO_CERT:
      resource_id = IDR_PAGEINFO_WARNING_MAJOR;
      break;
    case WebsiteSettings::SITE_IDENTITY_STATUS_ERROR:
      resource_id = IDR_PAGEINFO_BAD;
      break;
    default:
      NOTREACHED();
      break;
  }
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(resource_id);
}

// static
const gfx::Image& WebsiteSettingsUI::GetConnectionIcon(
    WebsiteSettings::SiteConnectionStatus status) {
  int resource_id = IDR_PAGEINFO_INFO;
  switch (status) {
    case WebsiteSettings::SITE_CONNECTION_STATUS_UNKNOWN:
      break;
    case WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED:
      resource_id = IDR_PAGEINFO_GOOD;
      break;
    case WebsiteSettings::SITE_CONNECTION_STATUS_MIXED_CONTENT:
      resource_id = IDR_PAGEINFO_WARNING_MINOR;
      break;
    case WebsiteSettings::SITE_CONNECTION_STATUS_UNENCRYPTED:
      resource_id = IDR_PAGEINFO_WARNING_MAJOR;
      break;
    case WebsiteSettings::SITE_CONNECTION_STATUS_ENCRYPTED_ERROR:
      resource_id = IDR_PAGEINFO_BAD;
      break;
    default:
      NOTREACHED();
      break;
  }
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(resource_id);
}
