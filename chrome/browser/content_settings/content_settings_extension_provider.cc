// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_extension_provider.h"

#include "chrome/browser/content_settings/content_settings_details.h"
#include "chrome/browser/extensions/extension_content_settings_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_details.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"

namespace content_settings {

ExtensionProvider::ExtensionProvider(
    HostContentSettingsMap* map,
    ExtensionContentSettingsStore* extensions_settings,
    bool incognito)
    : map_(map),
      incognito_(incognito),
      extensions_settings_(extensions_settings) {
  extensions_settings_->AddObserver(this);
}

ExtensionProvider::~ExtensionProvider() {
  DCHECK(!map_);
}

ContentSetting ExtensionProvider::GetContentSetting(
    const GURL& embedded_url,
    const GURL& top_level_url,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier) const {
  // TODO(markusheintz): Instead of getting the effective setting every time
  // effective patterns could be cached in here.
  DCHECK(extensions_settings_);
  return extensions_settings_->GetEffectiveContentSetting(
      embedded_url,
      top_level_url,
      content_type,
      resource_identifier,
      incognito_);
}

void ExtensionProvider::GetAllContentSettingsRules(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    Rules* content_setting_rules) const {
  return extensions_settings_->GetContentSettingsForContentType(
      content_type, resource_identifier, incognito_, content_setting_rules);
}

void ExtensionProvider::ShutdownOnUIThread() {
  extensions_settings_->RemoveObserver(this);
  map_ = NULL;
}

void ExtensionProvider::NotifyObservers(
    const ContentSettingsDetails& details) {
  DCHECK(map_);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_CONTENT_SETTINGS_CHANGED,
      Source<HostContentSettingsMap>(map_),
      Details<const ContentSettingsDetails>(&details));
}

void ExtensionProvider::OnContentSettingChanged(
    const std::string& extension_id,
    bool incognito) {
  if (incognito_ != incognito)
    return;
  // TODO(markusheintz): Be more concise.
  ContentSettingsDetails details(ContentSettingsPattern(),
                                 ContentSettingsPattern(),
                                 CONTENT_SETTINGS_TYPE_DEFAULT,
                                 std::string());
  NotifyObservers(details);
}

}  // namespace content_settings
