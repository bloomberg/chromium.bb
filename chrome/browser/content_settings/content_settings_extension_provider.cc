// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_extension_provider.h"

#include "chrome/browser/extensions/extension_content_settings_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"

namespace content_settings {

ExtensionProvider::ExtensionProvider(
    ExtensionContentSettingsStore* extensions_settings,
    bool incognito)
    : incognito_(incognito),
      extensions_settings_(extensions_settings) {
  extensions_settings_->AddObserver(this);
}

ExtensionProvider::~ExtensionProvider() {
}

ContentSetting ExtensionProvider::GetContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier) const {
  // TODO(markusheintz): Instead of getting the effective setting every time
  // effective patterns could be cached in here.
  DCHECK(extensions_settings_);
  return extensions_settings_->GetEffectiveContentSetting(
      primary_url,
      secondary_url,
      content_type,
      resource_identifier,
      incognito_);
}

Value* ExtensionProvider::GetContentSettingValue(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier) const {
  // TODO(markusheintz): Change the ExtensionSettingsStore to use the
  // OriginIdentifierValueMap to allow arbitray |Value|s to be stored instead of
  // |ContentSetting|s.
  ContentSetting setting = GetContentSetting(
      primary_url,
      secondary_url,
      content_type,
      resource_identifier);
  if (setting == CONTENT_SETTING_DEFAULT)
    return NULL;
  return Value::CreateIntegerValue(setting);
}


void ExtensionProvider::GetAllContentSettingsRules(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    Rules* content_setting_rules) const {
  return extensions_settings_->GetContentSettingsForContentType(
      content_type, resource_identifier, incognito_, content_setting_rules);
}

void ExtensionProvider::ShutdownOnUIThread() {
  RemoveAllObservers();
  extensions_settings_->RemoveObserver(this);
}

void ExtensionProvider::OnContentSettingChanged(
    const std::string& extension_id,
    bool incognito) {
  if (incognito_ != incognito)
    return;
  // TODO(markusheintz): Be more concise.
  NotifyObservers(ContentSettingsPattern(),
                  ContentSettingsPattern(),
                  CONTENT_SETTINGS_TYPE_DEFAULT,
                  std::string());
}

}  // namespace content_settings
