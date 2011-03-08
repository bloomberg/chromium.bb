// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/clear_browser_data_handler.h"

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_details.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

ClearBrowserDataHandler::ClearBrowserDataHandler() : remover_(NULL) {
}

ClearBrowserDataHandler::~ClearBrowserDataHandler() {
  if (remover_) {
    remover_->RemoveObserver(this);
  }
}

void ClearBrowserDataHandler::Initialize() {
  clear_plugin_lso_data_enabled_.Init(prefs::kClearPluginLSODataEnabled,
                                      g_browser_process->local_state(),
                                      this);
  UpdateClearPluginLSOData();
}

void ClearBrowserDataHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  RegisterTitle(localized_strings, "clearBrowserDataOverlay",
                IDS_CLEAR_BROWSING_DATA_TITLE);

  localized_strings->SetString("clearBrowserDataLabel",
      l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_LABEL));
  localized_strings->SetString("deleteBrowsingHistoryCheckbox",
      l10n_util::GetStringUTF16(IDS_DEL_BROWSING_HISTORY_CHKBOX));
  localized_strings->SetString("deleteDownloadHistoryCheckbox",
      l10n_util::GetStringUTF16(IDS_DEL_DOWNLOAD_HISTORY_CHKBOX));
  localized_strings->SetString("deleteCacheCheckbox",
      l10n_util::GetStringUTF16(IDS_DEL_CACHE_CHKBOX));
  localized_strings->SetString("deleteCookiesCheckbox",
      l10n_util::GetStringUTF16(IDS_DEL_COOKIES_CHKBOX));
  localized_strings->SetString("deletePasswordsCheckbox",
      l10n_util::GetStringUTF16(IDS_DEL_PASSWORDS_CHKBOX));
  localized_strings->SetString("deleteFormDataCheckbox",
      l10n_util::GetStringUTF16(IDS_DEL_FORM_DATA_CHKBOX));
  localized_strings->SetString("clearBrowserDataCommit",
      l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_COMMIT));
  localized_strings->SetString("flashStorageSettings",
      l10n_util::GetStringUTF16(IDS_FLASH_STORAGE_SETTINGS));
  localized_strings->SetString("flash_storage_url",
      l10n_util::GetStringUTF16(IDS_FLASH_STORAGE_URL));
  localized_strings->SetString("clearDataDeleting",
      l10n_util::GetStringUTF16(IDS_CLEAR_DATA_DELETING));

  ListValue* time_list = new ListValue;
  for (int i = 0; i < 5; i++) {
    string16 label_string;
    switch (i) {
      case 0:
        label_string = l10n_util::GetStringUTF16(IDS_CLEAR_DATA_HOUR);
        break;
      case 1:
        label_string = l10n_util::GetStringUTF16(IDS_CLEAR_DATA_DAY);
        break;
      case 2:
        label_string = l10n_util::GetStringUTF16(IDS_CLEAR_DATA_WEEK);
        break;
      case 3:
        label_string = l10n_util::GetStringUTF16(IDS_CLEAR_DATA_4WEEKS);
        break;
      case 4:
        label_string = l10n_util::GetStringUTF16(IDS_CLEAR_DATA_EVERYTHING);
        break;
    }
    ListValue* option = new ListValue();
    option->Append(Value::CreateIntegerValue(i));
    option->Append(Value::CreateStringValue(label_string));
    time_list->Append(option);
  }
  localized_strings->Set("clearBrowserDataTimeList", time_list);
}

void ClearBrowserDataHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  DCHECK(web_ui_);
  web_ui_->RegisterMessageCallback("performClearBrowserData",
      NewCallback(this, &ClearBrowserDataHandler::HandleClearBrowserData));
}

void ClearBrowserDataHandler::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::PREF_CHANGED: {
      const std::string& pref_name = *Details<std::string>(details).ptr();
      if (pref_name == prefs::kClearPluginLSODataEnabled)
        UpdateClearPluginLSOData();
      else
        OptionsPageUIHandler::Observe(type, source, details);
      break;
    }

    default:
      OptionsPageUIHandler::Observe(type, source, details);
  }
}

void ClearBrowserDataHandler::HandleClearBrowserData(const ListValue* value) {
  Profile* profile = web_ui_->GetProfile();
  PrefService* prefs = profile->GetPrefs();

  int remove_mask = 0;
  if (prefs->GetBoolean(prefs::kDeleteBrowsingHistory))
    remove_mask |= BrowsingDataRemover::REMOVE_HISTORY;
  if (prefs->GetBoolean(prefs::kDeleteDownloadHistory))
    remove_mask |= BrowsingDataRemover::REMOVE_DOWNLOADS;
  if (prefs->GetBoolean(prefs::kDeleteCache))
    remove_mask |= BrowsingDataRemover::REMOVE_CACHE;
  if (prefs->GetBoolean(prefs::kDeleteCookies)) {
    remove_mask |= BrowsingDataRemover::REMOVE_COOKIES;
    if (clear_plugin_lso_data_enabled_.GetValue())
      remove_mask |= BrowsingDataRemover::REMOVE_LSO_DATA;
  }
  if (prefs->GetBoolean(prefs::kDeletePasswords))
    remove_mask |= BrowsingDataRemover::REMOVE_PASSWORDS;
  if (prefs->GetBoolean(prefs::kDeleteFormData))
    remove_mask |= BrowsingDataRemover::REMOVE_FORM_DATA;

  int period_selected = prefs->GetInteger(prefs::kDeleteTimePeriod);

  FundamentalValue state(true);
  web_ui_->CallJavascriptFunction(L"ClearBrowserDataOverlay.setClearingState",
                                  state);

  // BrowsingDataRemover deletes itself when done.
  remover_ = new BrowsingDataRemover(profile,
      static_cast<BrowsingDataRemover::TimePeriod>(period_selected),
      base::Time());
  remover_->AddObserver(this);
  remover_->Remove(remove_mask);
}

void ClearBrowserDataHandler::UpdateClearPluginLSOData() {
  int label_id = clear_plugin_lso_data_enabled_.GetValue() ?
      IDS_DEL_COOKIES_FLASH_CHKBOX :
      IDS_DEL_COOKIES_CHKBOX;
  scoped_ptr<Value> label(
      Value::CreateStringValue(l10n_util::GetStringUTF16(label_id)));
  web_ui_->CallJavascriptFunction(
      L"ClearBrowserDataOverlay.setClearLocalDataLabel", *label);
}

void ClearBrowserDataHandler::OnBrowsingDataRemoverDone() {
  // No need to remove ourselves as an observer as BrowsingDataRemover deletes
  // itself after we return.
  remover_ = NULL;
  DCHECK(web_ui_);
  web_ui_->CallJavascriptFunction(L"ClearBrowserDataOverlay.doneClearing");
}
