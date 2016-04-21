// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <blimp/engine/app/settings_manager.h>
#include "content/public/browser/render_view_host.h"
#include "content/public/common/web_preferences.h"

namespace blimp {
namespace engine {

SettingsManager::SettingsManager() {}

SettingsManager::~SettingsManager() {}

void SettingsManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SettingsManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void SettingsManager::UpdateWebkitPreferences(content::WebPreferences* prefs) {
  prefs->record_whole_document = settings_.record_whole_document;
  prefs->animation_policy = settings_.animation_policy;
}

const EngineSettings& SettingsManager::GetEngineSettings() const {
  return settings_;
}

void SettingsManager::UpdateEngineSettings(const EngineSettings& settings) {
  EngineSettings old_settings = settings_;
  settings_ = settings;

  if (settings_.record_whole_document != old_settings.record_whole_document) {
    // Notify the observers that the web preferences have changed.
    FOR_EACH_OBSERVER(Observer, observer_list_, OnWebPreferencesChanged());
  }
}

}  // namespace engine
}  // namespace blimp
