// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_internal_extension_provider.h"

#include "chrome/browser/content_settings/content_settings_rule.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
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
    if ((*it)->plugins().size() > 0)
      SetContentSettingForExtension(*it, CONTENT_SETTING_ALLOW);
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
      if (host->extension()->is_platform_app())
        SetContentSettingForExtension(host->extension(), CONTENT_SETTING_BLOCK);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const extensions::Extension* extension =
          content::Details<extensions::Extension>(details).ptr();
      if (extension->plugins().size() > 0)
        SetContentSettingForExtension(extension, CONTENT_SETTING_ALLOW);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const UnloadedExtensionInfo& info =
          *(content::Details<UnloadedExtensionInfo>(details).ptr());
      if (info.extension->plugins().size() > 0)
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
                             ResourceIdentifier(""));
    } else {
      value_map_.SetValue(primary_pattern,
                          secondary_pattern,
                          CONTENT_SETTINGS_TYPE_PLUGINS,
                          ResourceIdentifier(""),
                          Value::CreateIntegerValue(setting));
    }
  }
  NotifyObservers(primary_pattern,
                  secondary_pattern,
                  CONTENT_SETTINGS_TYPE_PLUGINS,
                  ResourceIdentifier(""));
}

}  // namespace content_settings
