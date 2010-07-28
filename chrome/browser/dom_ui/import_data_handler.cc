// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/import_data_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/values.h"
#include "base/callback.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "chrome/browser/importer/importer_data_types.h"

ImportDataHandler::ImportDataHandler() {
}

ImportDataHandler::~ImportDataHandler() {
}

void ImportDataHandler::Initialize() {
  importer_host_ = new ImporterHost();
  DetectSupportedBrowsers();
}

void ImportDataHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString(L"import_data_title",
      l10n_util::GetString(IDS_IMPORT_SETTINGS_TITLE));
  localized_strings->SetString(L"import_from_label",
      l10n_util::GetString(IDS_IMPORT_FROM_LABEL));
  localized_strings->SetString(L"import_commit",
      l10n_util::GetString(IDS_IMPORT_COMMIT));
  localized_strings->SetString(L"import_description",
      l10n_util::GetString(IDS_IMPORT_ITEMS_LABEL));
  localized_strings->SetString(L"import_favorites",
      l10n_util::GetString(IDS_IMPORT_FAVORITES_CHKBOX));
  localized_strings->SetString(L"import_search",
      l10n_util::GetString(IDS_IMPORT_SEARCH_ENGINES_CHKBOX));
  localized_strings->SetString(L"import_passwords",
      l10n_util::GetString(IDS_IMPORT_PASSWORDS_CHKBOX));
  localized_strings->SetString(L"import_history",
      l10n_util::GetString(IDS_IMPORT_HISTORY_CHKBOX));
}

void ImportDataHandler::RegisterMessages() {
}

void ImportDataHandler::DetectSupportedBrowsers() {
  ListValue supported_browsers;
  int profiles_count = importer_host_->GetAvailableProfileCount();

  if (profiles_count > 0) {
    for (int i = 0; i < profiles_count; i++) {
      std::wstring profile = importer_host_->GetSourceProfileNameAt(i);
      DictionaryValue* entry = new DictionaryValue();
      entry->SetString(L"name", profile);
      entry->SetInteger(L"index", i);
      supported_browsers.Append(entry);
    }
  } else {
    DictionaryValue* entry = new DictionaryValue();
    entry->SetString(L"name", l10n_util::GetString(IDS_IMPORT_FROM_LABEL));
    entry->SetInteger(L"index", 0);
    supported_browsers.Append(entry);
  }

  dom_ui_->CallJavascriptFunction(
      L"ImportDataOverlay.updateSupportedBrowsers", supported_browsers);
}
