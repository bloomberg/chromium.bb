// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/website_settings_registry.h"

#include "base/logging.h"

namespace {

base::LazyInstance<content_settings::WebsiteSettingsRegistry>::Leaky
    g_instance = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace content_settings {

// static
WebsiteSettingsRegistry* WebsiteSettingsRegistry::GetInstance() {
  return g_instance.Pointer();
}

WebsiteSettingsRegistry::WebsiteSettingsRegistry() {
  website_settings_info_.resize(CONTENT_SETTINGS_NUM_TYPES);

  // TODO(raymes): This registration code should not have to be in a single
  // location. It should be possible to register a setting from the code
  // associated with it.

  Register(CONTENT_SETTINGS_TYPE_COOKIES, "cookies");
  Register(CONTENT_SETTINGS_TYPE_IMAGES, "images");
  Register(CONTENT_SETTINGS_TYPE_JAVASCRIPT, "javascript");
  Register(CONTENT_SETTINGS_TYPE_PLUGINS, "plugins");
  Register(CONTENT_SETTINGS_TYPE_POPUPS, "popups");
  Register(CONTENT_SETTINGS_TYPE_GEOLOCATION, "geolocation");
  Register(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, "notifications");
  Register(CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
           "auto-select-certificate");
  Register(CONTENT_SETTINGS_TYPE_FULLSCREEN, "fullscreen");
  Register(CONTENT_SETTINGS_TYPE_MOUSELOCK, "mouselock");
  Register(CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, "mixed-script");
  Register(CONTENT_SETTINGS_TYPE_MEDIASTREAM, "media-stream");
  Register(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, "media-stream-mic");
  Register(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, "media-stream-camera");
  Register(CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS,
           "register-protocol-handler");
  Register(CONTENT_SETTINGS_TYPE_PPAPI_BROKER, "ppapi-broker");
  Register(CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS,
           "multiple-automatic-downloads");
  Register(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, "midi-sysex");
  Register(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING, "push-messaging");
  Register(CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, "ssl-cert-decisions");
#if defined(OS_WIN)
  Register(CONTENT_SETTINGS_TYPE_METRO_SWITCH_TO_DESKTOP,
           "metro-switch-to-desktop");
#elif defined(OS_ANDROID) || defined(OS_CHROMEOS)
  Register(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
           "protected-media-identifier");
#endif
  Register(CONTENT_SETTINGS_TYPE_APP_BANNER, "app-banner");
  Register(CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, "site-engagement");
  Register(CONTENT_SETTINGS_TYPE_DURABLE_STORAGE, "durable-storage");
}

WebsiteSettingsRegistry::~WebsiteSettingsRegistry() {}

const WebsiteSettingsInfo* WebsiteSettingsRegistry::Get(
    ContentSettingsType type) const {
  DCHECK_GE(type, 0);
  DCHECK_LT(type, static_cast<int>(website_settings_info_.size()));
  return website_settings_info_[type];
}

const WebsiteSettingsInfo* WebsiteSettingsRegistry::GetByName(
    const std::string& name) const {
  for (const auto& info : website_settings_info_) {
    if (info && info->name() == name)
      return info;
  }
  return nullptr;
}

void WebsiteSettingsRegistry::Register(ContentSettingsType type,
                                       const std::string& name) {
  DCHECK_GE(type, 0);
  DCHECK_LT(type, static_cast<int>(website_settings_info_.size()));
  website_settings_info_[type] = new WebsiteSettingsInfo(type, name);
}

}  // namespace content_settings
