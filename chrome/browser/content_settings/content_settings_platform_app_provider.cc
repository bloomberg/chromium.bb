// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_platform_app_provider.h"

#include "chrome/browser/content_settings/content_settings_rule.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"

namespace content_settings {

PlatformAppProvider::PlatformAppProvider() {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_CREATED,
                 content::NotificationService::AllSources());
}

PlatformAppProvider::~PlatformAppProvider() {}

RuleIterator* PlatformAppProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  return value_map_.GetRuleIterator(content_type, resource_identifier, &lock_);
}

bool PlatformAppProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    Value* value) {
  return false;
}

void PlatformAppProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {}

void PlatformAppProvider::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_EXTENSION_HOST_CREATED);
  const ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
  if (!host->extension()->is_platform_app())
    return;

  base::AutoLock lock(lock_);
  scoped_ptr<ContentSettingsPattern::BuilderInterface> pattern_builder(
      ContentSettingsPattern::CreateBuilder(false));
  pattern_builder->WithScheme(chrome::kExtensionScheme);
  pattern_builder->WithHost(host->extension()->id());
  pattern_builder->WithPathWildcard();

  value_map_.SetValue(pattern_builder->Build(),
                      ContentSettingsPattern::Wildcard(),
                      CONTENT_SETTINGS_TYPE_PLUGINS,
                      ResourceIdentifier(""),
                      Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
}

void PlatformAppProvider::ShutdownOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  registrar_.RemoveAll();
}

}  // namespace content_settings
