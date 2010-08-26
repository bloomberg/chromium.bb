// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/import_data_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/callback.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "chrome/browser/importer/importer_data_types.h"

ImportDataHandler::ImportDataHandler() {
}

ImportDataHandler::~ImportDataHandler() {
  if (importer_host_ != NULL) {
    importer_host_->SetObserver(NULL);
    importer_host_ = NULL;
  }
}

void ImportDataHandler::Initialize() {
}

void ImportDataHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString("import_data_title",
      l10n_util::GetStringUTF16(IDS_IMPORT_SETTINGS_TITLE));
  localized_strings->SetString("import_from_label",
      l10n_util::GetStringUTF16(IDS_IMPORT_FROM_LABEL));
  localized_strings->SetString("import_commit",
      l10n_util::GetStringUTF16(IDS_IMPORT_COMMIT));
  localized_strings->SetString("import_description",
      l10n_util::GetStringUTF16(IDS_IMPORT_ITEMS_LABEL));
  localized_strings->SetString("import_favorites",
      l10n_util::GetStringUTF16(IDS_IMPORT_FAVORITES_CHKBOX));
  localized_strings->SetString("import_search",
      l10n_util::GetStringUTF16(IDS_IMPORT_SEARCH_ENGINES_CHKBOX));
  localized_strings->SetString("import_passwords",
      l10n_util::GetStringUTF16(IDS_IMPORT_PASSWORDS_CHKBOX));
  localized_strings->SetString("import_history",
      l10n_util::GetStringUTF16(IDS_IMPORT_HISTORY_CHKBOX));
  localized_strings->SetString("no_profile_found",
      l10n_util::GetStringUTF16(IDS_IMPORT_NO_PROFILE_FOUND));
}

void ImportDataHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(
      "loadImporter", NewCallback(this, &ImportDataHandler::LoadImporter));
  dom_ui_->RegisterMessageCallback(
      "importData", NewCallback(this, &ImportDataHandler::ImportData));
}

void ImportDataHandler::LoadImporter(const ListValue* args) {
  importer_host_ = new ImporterHost();
  DetectSupportedBrowsers();
}

void ImportDataHandler::DetectSupportedBrowsers() {
  ListValue supported_browsers;
  int profiles_count = importer_host_->GetAvailableProfileCount();

  if (profiles_count > 0) {
    for (int i = 0; i < profiles_count; i++) {
      string16 profile =
          WideToUTF16Hack(importer_host_->GetSourceProfileNameAt(i));
      DictionaryValue* entry = new DictionaryValue();
      entry->SetString("name", profile);
      entry->SetInteger("index", i);
      supported_browsers.Append(entry);
    }
  }

  dom_ui_->CallJavascriptFunction(
      L"options.ImportDataOverlay.updateSupportedBrowsers",
      supported_browsers);
}

void ImportDataHandler::ImportData(const ListValue* args) {
  std::string string_value = WideToUTF8(ExtractStringValue(args));
  CHECK(!string_value.empty());
  int browser_index = string_value[0] - '0';

  uint16 items = importer::NONE;
  if (string_value[1] == '1') {
    items |= importer::FAVORITES;
  }
  if (string_value[2] == '1') {
    items |= importer::SEARCH_ENGINES;
  }
  if (string_value[3] == '1') {
    items |= importer::PASSWORDS;
  }
  if (string_value[4] == '1') {
    items |= importer::HISTORY;
  }

  const ProfileInfo& source_profile =
      importer_host_->GetSourceProfileInfoAt(browser_index);
  Profile* profile = dom_ui_->GetProfile();

  FundamentalValue state(true);
  dom_ui_->CallJavascriptFunction(
      L"ImportDataOverlay.setImportingState", state);

  importer_host_->SetObserver(this);
  importer_host_->StartImportSettings(source_profile, profile, items,
                                      new ProfileWriter(profile), false);
}

void ImportDataHandler::ImportStarted() {
}

void ImportDataHandler::ImportItemStarted(importer::ImportItem item) {
}

void ImportDataHandler::ImportItemEnded(importer::ImportItem item) {
}

void ImportDataHandler::ImportEnded() {
  dom_ui_->CallJavascriptFunction(L"ImportDataOverlay.dismiss");
  importer_host_ = NULL;
}
