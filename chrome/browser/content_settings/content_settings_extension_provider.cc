// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_extension_provider.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_store.h"
#include "chrome/common/content_settings_pattern.h"

namespace content_settings {

ExtensionProvider::ExtensionProvider(
    extensions::ContentSettingsStore* extensions_settings,
    bool incognito)
    : incognito_(incognito),
      extensions_settings_(extensions_settings) {
  extensions_settings_->AddObserver(this);
}

ExtensionProvider::~ExtensionProvider() {
}

RuleIterator* ExtensionProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  return extensions_settings_->GetRuleIterator(content_type,
                                               resource_identifier,
                                               incognito);
}

bool ExtensionProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    Value* value) {
  return false;
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
