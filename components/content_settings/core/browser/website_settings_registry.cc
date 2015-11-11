// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/website_settings_registry.h"

#include "base/logging.h"
#include "components/content_settings/core/common/content_settings.h"

namespace {

base::LazyInstance<content_settings::WebsiteSettingsRegistry> g_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace content_settings {

// static
WebsiteSettingsRegistry* WebsiteSettingsRegistry::GetInstance() {
  return g_instance.Pointer();
}

WebsiteSettingsRegistry::WebsiteSettingsRegistry() {
  Init();
}

WebsiteSettingsRegistry::~WebsiteSettingsRegistry() {}

void WebsiteSettingsRegistry::ResetForTest() {
  website_settings_info_.clear();
  Init();
}

const WebsiteSettingsInfo* WebsiteSettingsRegistry::Get(
    ContentSettingsType type) const {
  const auto& it = website_settings_info_.find(type);
  if (it != website_settings_info_.end())
    return it->second;
  return nullptr;
}

const WebsiteSettingsInfo* WebsiteSettingsRegistry::GetByName(
    const std::string& name) const {
  for (const auto& entry : website_settings_info_) {
    if (entry.second->name() == name)
      return entry.second;
  }
  return nullptr;
}

const WebsiteSettingsInfo* WebsiteSettingsRegistry::Register(
    ContentSettingsType type,
    const std::string& name,
    scoped_ptr<base::Value> initial_default_value,
    WebsiteSettingsInfo::SyncStatus sync_status,
    WebsiteSettingsInfo::LossyStatus lossy_status) {
  WebsiteSettingsInfo* info = new WebsiteSettingsInfo(
      type, name, initial_default_value.Pass(), sync_status, lossy_status);
  website_settings_info_.set(info->type(), make_scoped_ptr(info));
  return info;
}

WebsiteSettingsRegistry::const_iterator WebsiteSettingsRegistry::begin() const {
  return const_iterator(website_settings_info_.begin());
}

WebsiteSettingsRegistry::const_iterator WebsiteSettingsRegistry::end() const {
  return const_iterator(website_settings_info_.end());
}

void WebsiteSettingsRegistry::Init() {
  // TODO(raymes): This registration code should not have to be in a single
  // location. It should be possible to register a setting from the code
  // associated with it.

  // WARNING: The string names of the permissions passed in below are used to
  // generate preference names and should never be changed!

  // Website settings.
  Register(CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
           "auto-select-certificate", nullptr, WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::NOT_LOSSY);
  Register(CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, "ssl-cert-decisions",
           nullptr, WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::NOT_LOSSY);
  Register(CONTENT_SETTINGS_TYPE_APP_BANNER, "app-banner", nullptr,
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY);
  Register(CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, "site-engagement", nullptr,
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY);
  Register(CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA, "usb-chooser-data", nullptr,
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY);
}

}  // namespace content_settings
