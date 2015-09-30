// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_registry.h"

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"

#if defined(ENABLE_PLUGINS)
#include "components/content_settings/core/browser/plugins_field_trial.h"
#endif

namespace content_settings {

namespace {

base::LazyInstance<content_settings::ContentSettingsRegistry> g_instance =
    LAZY_INSTANCE_INITIALIZER;

// These functions return a vector of schemes in which various permissions will
// be whitelisted.
const std::vector<std::string>& NoWhitelistedSchemes() {
  CR_DEFINE_STATIC_LOCAL(std::vector<std::string>, kNoWhitelistedSchemes, ());
  return kNoWhitelistedSchemes;
}

const std::vector<std::string>& WhitelistedForWebUI() {
  CR_DEFINE_STATIC_LOCAL(std::vector<std::string>, kWhitelistedForWebUI, ());
  if (kWhitelistedForWebUI.size() == 0) {
    kWhitelistedForWebUI.push_back(kChromeDevToolsScheme);
    kWhitelistedForWebUI.push_back(kChromeUIScheme);
  }
  return kWhitelistedForWebUI;
}

const std::vector<std::string>& WhitelistedForWebUIAndExtensions() {
  CR_DEFINE_STATIC_LOCAL(std::vector<std::string>,
                         kWhitelistedForWebUIAndExtensions, ());
  if (kWhitelistedForWebUIAndExtensions.size() == 0) {
    kWhitelistedForWebUIAndExtensions = WhitelistedForWebUI();
#if defined(ENABLE_EXTENSIONS)
    kWhitelistedForWebUIAndExtensions.push_back(kExtensionScheme);
#endif
  }
  return kWhitelistedForWebUIAndExtensions;
}

ContentSetting GetDefaultPluginsContentSetting() {
#if defined(ENABLE_PLUGINS)
  return PluginsFieldTrial::GetDefaultPluginsContentSetting();
#else
  return CONTENT_SETTING_BLOCK;
#endif
}

}  // namespace

// static
ContentSettingsRegistry* ContentSettingsRegistry::GetInstance() {
  return g_instance.Pointer();
}

ContentSettingsRegistry::ContentSettingsRegistry()
    : ContentSettingsRegistry(WebsiteSettingsRegistry::GetInstance()) {}

ContentSettingsRegistry::ContentSettingsRegistry(
    WebsiteSettingsRegistry* website_settings_registry)
    // This object depends on WebsiteSettingsRegistry, so get it first so that
    // they will be destroyed in reverse order.
    : website_settings_registry_(website_settings_registry) {
  Init();
}

void ContentSettingsRegistry::ResetForTest() {
  website_settings_registry_->ResetForTest();
  content_settings_info_.clear();
  Init();
}

ContentSettingsRegistry::~ContentSettingsRegistry() {}

const ContentSettingsInfo* ContentSettingsRegistry::Get(
    ContentSettingsType type) const {
  const auto& it = content_settings_info_.find(type);
  if (it != content_settings_info_.end())
    return it->second;
  return nullptr;
}

ContentSettingsRegistry::const_iterator ContentSettingsRegistry::begin() const {
  return const_iterator(content_settings_info_.begin());
}

ContentSettingsRegistry::const_iterator ContentSettingsRegistry::end() const {
  return const_iterator(content_settings_info_.end());
}

void ContentSettingsRegistry::Init() {
  // TODO(raymes): This registration code should not have to be in a single
  // location. It should be possible to register a setting from the code
  // associated with it.

  // WARNING: The string names of the permissions passed in below are used to
  // generate preference names and should never be changed!

  // Content settings (those with allow/block/ask/etc. values).
  Register(CONTENT_SETTINGS_TYPE_COOKIES, "cookies", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::SYNCABLE, WhitelistedForWebUI());
  Register(CONTENT_SETTINGS_TYPE_IMAGES, "images", CONTENT_SETTING_ALLOW,
           WebsiteSettingsInfo::SYNCABLE, WhitelistedForWebUIAndExtensions());
  Register(CONTENT_SETTINGS_TYPE_JAVASCRIPT, "javascript",
           CONTENT_SETTING_ALLOW, WebsiteSettingsInfo::SYNCABLE,
           WhitelistedForWebUIAndExtensions());
  Register(CONTENT_SETTINGS_TYPE_PLUGINS, "plugins",
           GetDefaultPluginsContentSetting(),
           WebsiteSettingsInfo::SYNCABLE, WhitelistedForWebUI());
  Register(CONTENT_SETTINGS_TYPE_POPUPS, "popups", CONTENT_SETTING_BLOCK,
           WebsiteSettingsInfo::SYNCABLE, WhitelistedForWebUIAndExtensions());
  Register(CONTENT_SETTINGS_TYPE_GEOLOCATION, "geolocation",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           NoWhitelistedSchemes());
  Register(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, "notifications",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           NoWhitelistedSchemes());
  Register(CONTENT_SETTINGS_TYPE_FULLSCREEN, "fullscreen", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::SYNCABLE, WhitelistedForWebUI());
  Register(CONTENT_SETTINGS_TYPE_MOUSELOCK, "mouselock", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::SYNCABLE, WhitelistedForWebUI());
  Register(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, "media-stream-mic",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedForWebUI());
  Register(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, "media-stream-camera",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedForWebUI());
  Register(CONTENT_SETTINGS_TYPE_PPAPI_BROKER, "ppapi-broker",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           WhitelistedForWebUI());
  Register(CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, "automatic-downloads",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::SYNCABLE,
           WhitelistedForWebUIAndExtensions());
  Register(CONTENT_SETTINGS_TYPE_MIDI_SYSEX, "midi-sysex", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::SYNCABLE, NoWhitelistedSchemes());
  Register(CONTENT_SETTINGS_TYPE_PUSH_MESSAGING, "push-messaging",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::SYNCABLE,
           NoWhitelistedSchemes());
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  Register(CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER,
           "protected-media-identifier", CONTENT_SETTING_ASK,
           WebsiteSettingsInfo::UNSYNCABLE, NoWhitelistedSchemes());
#endif
  Register(CONTENT_SETTINGS_TYPE_DURABLE_STORAGE, "durable-storage",
           CONTENT_SETTING_ASK, WebsiteSettingsInfo::UNSYNCABLE,
           NoWhitelistedSchemes());

  // Content settings that aren't used to store any data. TODO(raymes): use a
  // different mechanism rather than content settings to represent these.
  // Since nothing is stored in them, there is no real point in them being a
  // content setting.
  Register(CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, "protocol-handler",
           CONTENT_SETTING_DEFAULT, WebsiteSettingsInfo::UNSYNCABLE,
           NoWhitelistedSchemes());
  Register(CONTENT_SETTINGS_TYPE_MIXEDSCRIPT, "mixed-script",
           CONTENT_SETTING_DEFAULT, WebsiteSettingsInfo::UNSYNCABLE,
           NoWhitelistedSchemes());

  // Deprecated.
  Register(CONTENT_SETTINGS_TYPE_MEDIASTREAM, "media-stream",
           CONTENT_SETTING_DEFAULT, WebsiteSettingsInfo::UNSYNCABLE,
           NoWhitelistedSchemes());
}

void ContentSettingsRegistry::Register(
    ContentSettingsType type,
    const std::string& name,
    ContentSetting initial_default_value,
    WebsiteSettingsInfo::SyncStatus sync_status,
    const std::vector<std::string>& whitelisted_schemes) {
  // Ensure that nothing has been registered yet for the given type.
  DCHECK(!website_settings_registry_->Get(type));
  scoped_ptr<base::Value> default_value(
      new base::FundamentalValue(static_cast<int>(initial_default_value)));
  const WebsiteSettingsInfo* website_settings_info =
      website_settings_registry_->Register(type, name, default_value.Pass(),
                                           sync_status,
                                           WebsiteSettingsInfo::NOT_LOSSY);
  DCHECK(!ContainsKey(content_settings_info_, type));
  content_settings_info_.set(
      type, make_scoped_ptr(new ContentSettingsInfo(website_settings_info,
                                                    whitelisted_schemes)));
}

}  // namespace content_settings
