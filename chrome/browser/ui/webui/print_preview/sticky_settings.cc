// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/sticky_settings.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "printing/page_size_margins.h"

namespace printing {

const char kSettingSavePath[] = "savePath";
const char kSettingAppState[] = "appState";

StickySettings::StickySettings() {}

StickySettings::~StickySettings() {}

void StickySettings::StoreAppState(const std::string& data) {
  printer_app_state_.reset(new std::string(data));
}

void StickySettings::StoreSavePath(const base::FilePath& path) {
  save_path_.reset(new base::FilePath(path));
}

void StickySettings::SaveInPrefs(PrefService* prefs) {
  DCHECK(prefs);
  if (prefs) {
    scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue);
    if (save_path_.get())
      value->SetString(printing::kSettingSavePath, save_path_->value());
    if (printer_app_state_.get())
      value->SetString(printing::kSettingAppState,
          *printer_app_state_);
    prefs->Set(prefs::kPrintPreviewStickySettings, *value);
  }
}

void StickySettings::RestoreFromPrefs(PrefService* prefs) {
  DCHECK(prefs);
  if (prefs) {
    const base::DictionaryValue* value =
        prefs->GetDictionary(prefs::kPrintPreviewStickySettings);

    base::FilePath::StringType save_path;
    if (value->GetString(printing::kSettingSavePath, &save_path))
      save_path_.reset(new base::FilePath(save_path));
    std::string buffer;
    if (value->GetString(printing::kSettingAppState, &buffer))
      printer_app_state_.reset(new std::string(buffer));
  }
}

void StickySettings::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      prefs::kPrintPreviewStickySettings,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

std::string* StickySettings::printer_app_state() {
  return printer_app_state_.get();
}

base::FilePath* StickySettings::save_path() {
  return save_path_.get();
}

} // namespace printing
