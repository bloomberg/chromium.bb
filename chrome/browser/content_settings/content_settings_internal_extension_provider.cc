// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_internal_extension_provider.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/content_settings_rule.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/extensions/api/plugins/plugins_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/features/simple_feature.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "extensions/common/constants.h"

using extensions::UnloadedExtensionInfo;

namespace content_settings {

InternalExtensionProvider::InternalExtensionProvider(
    ExtensionService* extension_service)
    : registrar_(new content::NotificationRegistrar) {
  // Whitelist all extensions loaded so far.
  const ExtensionSet* extensions = extension_service->extensions();
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    if (extensions::PluginInfo::HasPlugins(it->get()))
      SetContentSettingForExtension(it->get(), CONTENT_SETTING_ALLOW);
  }
  Profile* profile = extension_service->profile();
  registrar_->Add(this, chrome::NOTIFICATION_EXTENSION_HOST_CREATED,
                  content::Source<Profile>(profile));
  registrar_->Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                  content::Source<Profile>(profile));
  registrar_->Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                  content::Source<Profile>(profile));
}

InternalExtensionProvider::~InternalExtensionProvider() {
  DCHECK(!registrar_.get());
}

RuleIterator* InternalExtensionProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  return value_map_.GetRuleIterator(content_type, resource_identifier, &lock_);
}

bool InternalExtensionProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    Value* value) {
  return false;
}

void InternalExtensionProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {}

void InternalExtensionProvider::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_CREATED: {
      const extensions::ExtensionHost* host =
          content::Details<extensions::ExtensionHost>(details).ptr();
      if (host->extension()->is_platform_app()) {
        SetContentSettingForExtension(host->extension(), CONTENT_SETTING_BLOCK);

        // White-list CRD's v2 app, until crbug.com/134216 is complete.
        const char* kAppWhitelist[] = {
          "2775E568AC98F9578791F1EAB65A1BF5F8CEF414",
          "4AA3C5D69A4AECBD236CAD7884502209F0F5C169",
          "97B23E01B2AA064E8332EE43A7A85C628AADC3F2",
          "9E930B2B5EABA6243AE6C710F126E54688E8FAF6",
          "C449A798C495E6CF7D6AF10162113D564E67AD12",
          "E410CDAB2C6E6DD408D731016CECF2444000A912",
          "EBA908206905323CECE6DC4B276A58A0F4AC573F"
        };
        if (extensions::SimpleFeature::IsIdInWhitelist(
                host->extension()->id(),
                std::set<std::string>(
                    kAppWhitelist, kAppWhitelist + arraysize(kAppWhitelist)))) {
          SetContentSettingForExtensionAndResource(
              host->extension(),
              ChromeContentClient::kRemotingViewerPluginPath,
              CONTENT_SETTING_ALLOW);
        }
      }

      break;
    }
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const extensions::Extension* extension =
          content::Details<extensions::Extension>(details).ptr();
      if (extensions::PluginInfo::HasPlugins(extension))
        SetContentSettingForExtension(extension, CONTENT_SETTING_ALLOW);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const UnloadedExtensionInfo& info =
          *(content::Details<UnloadedExtensionInfo>(details).ptr());
      if (extensions::PluginInfo::HasPlugins(info.extension))
        SetContentSettingForExtension(info.extension, CONTENT_SETTING_DEFAULT);
      break;
    }
    default:
      NOTREACHED();
  }
}

void InternalExtensionProvider::ShutdownOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  RemoveAllObservers();
  registrar_.reset();
}

void InternalExtensionProvider::SetContentSettingForExtension(
    const extensions::Extension* extension,
    ContentSetting setting) {
  SetContentSettingForExtensionAndResource(
      extension, ResourceIdentifier(), setting);
}

void InternalExtensionProvider::SetContentSettingForExtensionAndResource(
    const extensions::Extension* extension,
    const ResourceIdentifier& resource,
    ContentSetting setting) {
  scoped_ptr<ContentSettingsPattern::BuilderInterface> pattern_builder(
      ContentSettingsPattern::CreateBuilder(false));
  pattern_builder->WithScheme(extensions::kExtensionScheme);
  pattern_builder->WithHost(extension->id());
  pattern_builder->WithPathWildcard();

  ContentSettingsPattern primary_pattern = pattern_builder->Build();
  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::Wildcard();
  {
    base::AutoLock lock(lock_);
    if (setting == CONTENT_SETTING_DEFAULT) {
      value_map_.DeleteValue(primary_pattern,
                             secondary_pattern,
                             CONTENT_SETTINGS_TYPE_PLUGINS,
                             resource);
    } else {
      value_map_.SetValue(primary_pattern,
                          secondary_pattern,
                          CONTENT_SETTINGS_TYPE_PLUGINS,
                          resource,
                          Value::CreateIntegerValue(setting));
    }
  }
  NotifyObservers(primary_pattern,
                  secondary_pattern,
                  CONTENT_SETTINGS_TYPE_PLUGINS,
                  resource);
}

}  // namespace content_settings
