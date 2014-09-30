// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/website_settings_ui.h"

#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

const int kInvalidResourceID = -1;

// The resource id's for the strings that are displayed on the permissions
// button if the permission setting is managed by policy.
const int kPermissionButtonTextIDPolicyManaged[] = {
  kInvalidResourceID,
  IDS_WEBSITE_SETTINGS_BUTTON_TEXT_ALLOWED_BY_POLICY,
  IDS_WEBSITE_SETTINGS_BUTTON_TEXT_BLOCKED_BY_POLICY,
  kInvalidResourceID,
  kInvalidResourceID
};
COMPILE_ASSERT(arraysize(kPermissionButtonTextIDPolicyManaged) ==
               CONTENT_SETTING_NUM_SETTINGS,
               button_text_id_array_size_incorrect);

// The resource id's for the strings that are displayed on the permissions
// button if the permission setting is managed by an extension.
const int kPermissionButtonTextIDExtensionManaged[] = {
  kInvalidResourceID,
  IDS_WEBSITE_SETTINGS_BUTTON_TEXT_ALLOWED_BY_EXTENSION,
  IDS_WEBSITE_SETTINGS_BUTTON_TEXT_BLOCKED_BY_EXTENSION,
  kInvalidResourceID,
  kInvalidResourceID
};
COMPILE_ASSERT(arraysize(kPermissionButtonTextIDExtensionManaged) ==
               CONTENT_SETTING_NUM_SETTINGS,
               button_text_id_array_size_incorrect);

// The resource id's for the strings that are displayed on the permissions
// button if the permission setting is managed by the user.
const int kPermissionButtonTextIDUserManaged[] = {
  kInvalidResourceID,
  IDS_WEBSITE_SETTINGS_BUTTON_TEXT_ALLOWED_BY_USER,
  IDS_WEBSITE_SETTINGS_BUTTON_TEXT_BLOCKED_BY_USER,
  kInvalidResourceID,
  kInvalidResourceID
};
COMPILE_ASSERT(arraysize(kPermissionButtonTextIDUserManaged) ==
               CONTENT_SETTING_NUM_SETTINGS,
               button_text_id_array_size_incorrect);

// The resource id's for the strings that are displayed on the permissions
// button if the permission setting is the global default setting.
const int kPermissionButtonTextIDDefaultSetting[] = {
  kInvalidResourceID,
  IDS_WEBSITE_SETTINGS_BUTTON_TEXT_ALLOWED_BY_DEFAULT,
  IDS_WEBSITE_SETTINGS_BUTTON_TEXT_BLOCKED_BY_DEFAULT,
  IDS_WEBSITE_SETTINGS_BUTTON_TEXT_ASK_BY_DEFAULT,
  kInvalidResourceID
};
COMPILE_ASSERT(arraysize(kPermissionButtonTextIDDefaultSetting) ==
               CONTENT_SETTING_NUM_SETTINGS,
               button_text_id_array_size_incorrect);

}  // namespace

WebsiteSettingsUI::CookieInfo::CookieInfo()
    : allowed(-1), blocked(-1) {
}

WebsiteSettingsUI::PermissionInfo::PermissionInfo()
    : type(CONTENT_SETTINGS_TYPE_DEFAULT),
      setting(CONTENT_SETTING_DEFAULT),
      default_setting(CONTENT_SETTING_DEFAULT),
      source(content_settings::SETTING_SOURCE_NONE) {
}

WebsiteSettingsUI::IdentityInfo::IdentityInfo()
    : identity_status(WebsiteSettings::SITE_IDENTITY_STATUS_UNKNOWN),
      cert_id(0),
      connection_status(WebsiteSettings::SITE_CONNECTION_STATUS_UNKNOWN),
      show_ssl_decision_revoke_button(false) {
}

WebsiteSettingsUI::IdentityInfo::~IdentityInfo() {}

base::string16 WebsiteSettingsUI::IdentityInfo::GetIdentityStatusText() const {
  if (identity_status == WebsiteSettings::SITE_IDENTITY_STATUS_CERT ||
      identity_status ==  WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT) {
    return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_IDENTITY_VERIFIED);
  }
  if (identity_status ==
          WebsiteSettings::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT) {
    return l10n_util::GetStringUTF16(IDS_CERT_POLICY_PROVIDED_CERT_HEADER);
  }
  return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_IDENTITY_NOT_VERIFIED);
}

WebsiteSettingsUI::~WebsiteSettingsUI() {
}

// static
base::string16 WebsiteSettingsUI::PermissionTypeToUIString(
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
    case CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
      return l10n_util::GetStringUTF16(IDS_AUTOMATIC_DOWNLOADS_TAB_LABEL);
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_TYPE_MIDI_SYSEX);
    default:
      NOTREACHED();
      return base::string16();
  }
}

// static
base::string16 WebsiteSettingsUI::PermissionValueToUIString(
    ContentSetting value) {
  switch (value) {
    case CONTENT_SETTING_ALLOW:
      return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_PERMISSION_ALLOW);
    case CONTENT_SETTING_BLOCK:
      return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_PERMISSION_BLOCK);
    case CONTENT_SETTING_ASK:
      return l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_PERMISSION_ASK);
    default:
      NOTREACHED();
      return base::string16();
  }
}

// static
base::string16 WebsiteSettingsUI::PermissionActionToUIString(
      ContentSetting setting,
      ContentSetting default_setting,
      content_settings::SettingSource source) {
  ContentSetting effective_setting = setting;
  if (effective_setting == CONTENT_SETTING_DEFAULT)
    effective_setting = default_setting;

  const int* button_text_ids = NULL;
  switch (source) {
    case content_settings::SETTING_SOURCE_USER:
      if (setting == CONTENT_SETTING_DEFAULT)
        button_text_ids = kPermissionButtonTextIDDefaultSetting;
      else
        button_text_ids = kPermissionButtonTextIDUserManaged;
      break;
    case content_settings::SETTING_SOURCE_POLICY:
      button_text_ids = kPermissionButtonTextIDPolicyManaged;
      break;
    case content_settings::SETTING_SOURCE_EXTENSION:
      button_text_ids = kPermissionButtonTextIDExtensionManaged;
      break;
    case content_settings::SETTING_SOURCE_WHITELIST:
    case content_settings::SETTING_SOURCE_NONE:
    default:
      NOTREACHED();
      return base::string16();
  }
  int button_text_id = button_text_ids[effective_setting];
  DCHECK_NE(button_text_id, kInvalidResourceID);
  return l10n_util::GetStringUTF16(button_text_id);
}

// static
int WebsiteSettingsUI::GetPermissionIconID(ContentSettingsType type,
                                           ContentSetting setting) {
  bool use_blocked = (setting == CONTENT_SETTING_BLOCK);
  int resource_id = IDR_INFO;
  switch (type) {
    case CONTENT_SETTINGS_TYPE_IMAGES:
      resource_id = use_blocked ? IDR_BLOCKED_IMAGES : IDR_ALLOWED_IMAGES;
      break;
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
      resource_id =
          use_blocked ? IDR_BLOCKED_JAVASCRIPT : IDR_ALLOWED_JAVASCRIPT;
      break;
    case CONTENT_SETTINGS_TYPE_COOKIES:
      resource_id = use_blocked ? IDR_BLOCKED_COOKIES : IDR_ACCESSED_COOKIES;
      break;
    case CONTENT_SETTINGS_TYPE_POPUPS:
      resource_id = use_blocked ? IDR_BLOCKED_POPUPS : IDR_ALLOWED_POPUPS;
      break;
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      resource_id = use_blocked ? IDR_BLOCKED_PLUGINS : IDR_ALLOWED_PLUGINS;
      break;
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      resource_id = use_blocked ? IDR_BLOCKED_LOCATION : IDR_ALLOWED_LOCATION;
      break;
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      resource_id =
          use_blocked ? IDR_BLOCKED_NOTIFICATION : IDR_ALLOWED_NOTIFICATION;
      break;
    case CONTENT_SETTINGS_TYPE_FULLSCREEN:
      resource_id = IDR_ALLOWED_FULLSCREEN;
      break;
    case CONTENT_SETTINGS_TYPE_MOUSELOCK:
      resource_id =
          use_blocked ? IDR_BLOCKED_MOUSE_CURSOR : IDR_ALLOWED_MOUSE_CURSOR;
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM:
      resource_id = use_blocked ? IDR_BLOCKED_MEDIA : IDR_ASK_MEDIA;
      break;
    case CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
      resource_id = use_blocked ? IDR_BLOCKED_DOWNLOADS
                                : IDR_ALLOWED_DOWNLOADS;
      break;
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      resource_id = use_blocked ? IDR_BLOCKED_MIDI_SYSEX
                                : IDR_ALLOWED_MIDI_SYSEX;
      break;
    default:
      NOTREACHED();
      break;
  }
  return resource_id;
}

// static
const gfx::Image& WebsiteSettingsUI::GetPermissionIcon(
    const PermissionInfo& info) {
  ContentSetting setting = info.setting;
  if (setting == CONTENT_SETTING_DEFAULT)
    setting = info.default_setting;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(GetPermissionIconID(info.type, setting));
}

// static
int WebsiteSettingsUI::GetIdentityIconID(
    WebsiteSettings::SiteIdentityStatus status) {
  int resource_id = IDR_PAGEINFO_INFO;
  switch (status) {
    case WebsiteSettings::SITE_IDENTITY_STATUS_UNKNOWN:
      break;
    case WebsiteSettings::SITE_IDENTITY_STATUS_CERT:
    case WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT:
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
    case WebsiteSettings::SITE_IDENTITY_STATUS_ADMIN_PROVIDED_CERT:
      resource_id = IDR_PAGEINFO_ENTERPRISE_MANAGED;
      break;
    case WebsiteSettings::SITE_IDENTITY_STATUS_DEPRECATED_SIGNATURE_ALGORITHM:
      resource_id = IDR_PAGEINFO_WARNING_MINOR;
      break;
    default:
      NOTREACHED();
      break;
  }
  return resource_id;
}

// static
const gfx::Image& WebsiteSettingsUI::GetIdentityIcon(
    WebsiteSettings::SiteIdentityStatus status) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(GetIdentityIconID(status));
}

// static
int WebsiteSettingsUI::GetConnectionIconID(
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
  return resource_id;
}

// static
const gfx::Image& WebsiteSettingsUI::GetConnectionIcon(
    WebsiteSettings::SiteConnectionStatus status) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(GetConnectionIconID(status));
}

// static
int WebsiteSettingsUI::GetFirstVisitIconID(const base::string16& first_visit) {
  // FIXME(markusheintz): Display a minor warning icon if the page is visited
  // the first time.
  return IDR_PAGEINFO_INFO;
}

// static
const gfx::Image& WebsiteSettingsUI::GetFirstVisitIcon(
    const base::string16& first_visit) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  return rb.GetNativeImageNamed(GetFirstVisitIconID(first_visit));
}
