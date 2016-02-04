// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/website_settings_registry.h"

#include <utility>

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
    return it->second.get();
  return nullptr;
}

const WebsiteSettingsInfo* WebsiteSettingsRegistry::GetByName(
    const std::string& name) const {
  for (const auto& entry : website_settings_info_) {
    if (entry.second->name() == name)
      return entry.second.get();
  }
  return nullptr;
}

const WebsiteSettingsInfo* WebsiteSettingsRegistry::Register(
    ContentSettingsType type,
    const std::string& name,
    scoped_ptr<base::Value> initial_default_value,
    WebsiteSettingsInfo::SyncStatus sync_status,
    WebsiteSettingsInfo::LossyStatus lossy_status,
    WebsiteSettingsInfo::ScopingType scoping_type,
    WebsiteSettingsInfo::IncognitoBehavior incognito_behavior) {
  WebsiteSettingsInfo* info = new WebsiteSettingsInfo(
      type, name, std::move(initial_default_value), sync_status, lossy_status,
      scoping_type, incognito_behavior);
  website_settings_info_[info->type()] = make_scoped_ptr(info);
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
           WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::REQUESTING_DOMAIN_ONLY_SCOPE,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, "ssl-cert-decisions",
           nullptr, WebsiteSettingsInfo::UNSYNCABLE,
           WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::REQUESTING_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(CONTENT_SETTINGS_TYPE_APP_BANNER, "app-banner", nullptr,
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::REQUESTING_DOMAIN_ONLY_SCOPE,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, "site-engagement", nullptr,
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::LOSSY,
           WebsiteSettingsInfo::REQUESTING_ORIGIN_ONLY_SCOPE,
           WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);
  Register(CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA, "usb-chooser-data", nullptr,
           WebsiteSettingsInfo::UNSYNCABLE, WebsiteSettingsInfo::NOT_LOSSY,
           WebsiteSettingsInfo::REQUESTING_ORIGIN_AND_TOP_LEVEL_ORIGIN_SCOPE,
           WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO);
}

}  // namespace content_settings
