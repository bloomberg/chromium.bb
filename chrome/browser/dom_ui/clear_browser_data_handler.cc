// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/clear_browser_data_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/values.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

ClearBrowserDataHandler::ClearBrowserDataHandler() {
}

ClearBrowserDataHandler::~ClearBrowserDataHandler() {
}

void ClearBrowserDataHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString(L"clearBrowsingDataTitle",
      l10n_util::GetString(IDS_CLEAR_BROWSING_DATA_TITLE));
  localized_strings->SetString(L"clearBrowsingDataLabel",
      l10n_util::GetString(IDS_CLEAR_BROWSING_DATA_LABEL));
  localized_strings->SetString(L"clearBrowsingDataTimeLabel",
      l10n_util::GetString(IDS_CLEAR_BROWSING_DATA_TIME_LABEL));
  localized_strings->SetString(L"deleteBrowsingHistoryCheckbox",
      l10n_util::GetString(IDS_DEL_BROWSING_HISTORY_CHKBOX));
  localized_strings->SetString(L"deleteDownloadHistoryCheckbox",
      l10n_util::GetString(IDS_DEL_DOWNLOAD_HISTORY_CHKBOX));
  localized_strings->SetString(L"deleteCacheCheckbox",
      l10n_util::GetString(IDS_DEL_CACHE_CHKBOX));
  localized_strings->SetString(L"deleteCookiesCheckbox",
      l10n_util::GetString(IDS_DEL_COOKIES_CHKBOX));
  localized_strings->SetString(L"deletePasswordsCheckbox",
      l10n_util::GetString(IDS_DEL_PASSWORDS_CHKBOX));
  localized_strings->SetString(L"deleteFormDataCheckbox",
      l10n_util::GetString(IDS_DEL_FORM_DATA_CHKBOX));
  localized_strings->SetString(L"clearBrowsingDataCommit",
      l10n_util::GetString(IDS_CLEAR_BROWSING_DATA_COMMIT));
  localized_strings->SetString(L"flashStorageSettings",
      l10n_util::GetString(IDS_FLASH_STORAGE_SETTINGS));
  localized_strings->SetString(L"flash_storage_url",
      l10n_util::GetString(IDS_FLASH_STORAGE_URL));
  localized_strings->SetString(L"clearDataDeleting",
      l10n_util::GetString(IDS_CLEAR_DATA_DELETING));

  ListValue* time_list = new ListValue;
  for (int i = 0; i < 5; i++) {
    std::wstring label_string;
    switch (i) {
      case 0:
        label_string = l10n_util::GetString(IDS_CLEAR_DATA_HOUR);
        break;
      case 1:
        label_string = l10n_util::GetString(IDS_CLEAR_DATA_DAY);
        break;
      case 2:
        label_string = l10n_util::GetString(IDS_CLEAR_DATA_WEEK);
        break;
      case 3:
        label_string = l10n_util::GetString(IDS_CLEAR_DATA_4WEEKS);
        break;
      case 4:
        label_string = l10n_util::GetString(IDS_CLEAR_DATA_EVERYTHING);
        break;
    }
    ListValue* option = new ListValue();
    option->Append(Value::CreateIntegerValue(i));
    option->Append(Value::CreateStringValue(label_string));
    time_list->Append(option);
  }
  localized_strings->Set(L"clearBrowsingDataTimeList", time_list);
}

void ClearBrowserDataHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  DCHECK(dom_ui_);
  dom_ui_->RegisterMessageCallback("performClearBrowserData",
      NewCallback(this, &ClearBrowserDataHandler::HandleClearBrowserData));
}

void ClearBrowserDataHandler::HandleClearBrowserData(const Value* value) {
  Profile *profile = dom_ui_->GetProfile();
  PrefService *prefs = profile->GetPrefs();

  int remove_mask = 0;
  if (prefs->GetBoolean(prefs::kDeleteBrowsingHistory))
    remove_mask |= BrowsingDataRemover::REMOVE_HISTORY;
  if (prefs->GetBoolean(prefs::kDeleteDownloadHistory))
    remove_mask |= BrowsingDataRemover::REMOVE_DOWNLOADS;
  if (prefs->GetBoolean(prefs::kDeleteCache))
    remove_mask |= BrowsingDataRemover::REMOVE_CACHE;
  if (prefs->GetBoolean(prefs::kDeleteCookies))
    remove_mask |= BrowsingDataRemover::REMOVE_COOKIES;
  if (prefs->GetBoolean(prefs::kDeletePasswords))
    remove_mask |= BrowsingDataRemover::REMOVE_PASSWORDS;
  if (prefs->GetBoolean(prefs::kDeleteFormData))
    remove_mask |= BrowsingDataRemover::REMOVE_FORM_DATA;

  int period_selected = prefs->GetInteger(prefs::kDeleteTimePeriod);

  FundamentalValue state(true);
  dom_ui_->CallJavascriptFunction(L"clearBrowserDataSetClearingState", state);

  // BrowsingDataRemover deletes itself when done.
  remover_ = new BrowsingDataRemover(profile,
      static_cast<BrowsingDataRemover::TimePeriod>(period_selected),
      base::Time());
  remover_->AddObserver(this);
  remover_->Remove(remove_mask);
}

void ClearBrowserDataHandler::OnBrowsingDataRemoverDone() {
  // No need to remove ourselves as an observer as BrowsingDataRemover deletes
  // itself after we return.
  remover_ = NULL;
  DCHECK(dom_ui_);
  dom_ui_->CallJavascriptFunction(L"clearBrowserDataDismiss");
}

