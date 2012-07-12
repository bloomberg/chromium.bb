// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/website_settings_ui.h"

#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {
const int kInvalidResourceID = -1;
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

string16 WebsiteSettingsUI::IdentityInfo::GetIdentityStatusText() const {
  if (identity_status == WebsiteSettings::SITE_IDENTITY_STATUS_CERT ||
      identity_status == WebsiteSettings::SITE_IDENTITY_STATUS_DNSSEC_CERT ||
      identity_status ==  WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT) {
    return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_IDENTITY_VERIFIED);
  }
  return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_IDENTITY_NOT_VERIFIED);
}

WebsiteSettingsUI::~WebsiteSettingsUI() {
}

// static
string16 WebsiteSettingsUI::PermissionTypeToUIString(
      ContentSettingsType type) {
  switch (type) {
    case CONTENT_SETTINGS_TYPE_IMAGES:
     return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TYPE_IMAGES);
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
     return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TYPE_JAVASCRIPT);
    case CONTENT_SETTINGS_TYPE_POPUPS:
      return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TYPE_POPUPS);
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TYPE_PLUGINS);
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TYPE_LOCATION);
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      return l10n_util::GetStringUTF16(
          IDS_WEBSITE_SETTINGS_TYPE_NOTIFICATIONS);
    case CONTENT_SETTINGS_TYPE_FULLSCREEN:
      return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TYPE_FULLSCREEN);
    case CONTENT_SETTINGS_TYPE_MOUSELOCK:
      return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TYPE_MOUSELOCK);
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
      return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TYPE_MEDIASTREAM);
    default:
      NOTREACHED();
      return string16();
  }
}

// static
string16 WebsiteSettingsUI::PermissionValueToUIString(ContentSetting value) {
  switch (value) {
    case CONTENT_SETTING_ALLOW:
      return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_PERMISSION_ALLOW);
    case CONTENT_SETTING_BLOCK:
      return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_PERMISSION_BLOCK);
    case CONTENT_SETTING_ASK:
      return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_PERMISSION_ASK);
    default:
      NOTREACHED();
      return string16();
  }
}

// static
string16 WebsiteSettingsUI::PermissionActionToUIString(
      ContentSetting setting, ContentSetting default_setting) {
  ContentSetting setting_value;
  int setting_description_id = kInvalidResourceID;
  int action_description_id = kInvalidResourceID;

  // Check whether or not the default setting was used.
  if (setting == CONTENT_SETTING_DEFAULT) {
    setting_value = default_setting;
    setting_description_id = IDS_WEBSITE_SETTINGS_DEFAULT_SETTING;
  } else {
    setting_value = setting;
    setting_description_id = IDS_WEBSITE_SETTINGS_USER_SETTING;
  }

  // Determine the string to use to describe the action that was taken, e.g.
  // "Allowed", "Blocked", "Ask".
  switch (setting_value) {
    case CONTENT_SETTING_ALLOW:
      action_description_id = IDS_WEBSITE_SETTINGS_PERMISSION_ACTION_ALLOWED;
      break;
    case CONTENT_SETTING_BLOCK:
      action_description_id = IDS_WEBSITE_SETTINGS_PERMISSION_ACTION_BLOCKED;
      break;
    case CONTENT_SETTING_ASK:
      action_description_id = IDS_WEBSITE_SETTINGS_PERMISSION_ACTION_ASK;
      break;
    default:
      NOTREACHED();
  }
  return l10n_util::GetStringFUTF16(setting_description_id,
      l10n_util::GetStringUTF16(action_description_id));
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

// static
const gfx::Image& WebsiteSettingsUI::GetFirstVisitIcon(
    const string16& first_visit) {
  // FIXME(markusheintz): Display a minor warning icon if the page is visited
  // the first time.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(IDR_PAGEINFO_INFO);
}
