// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"

namespace settings {

ClearBrowsingDataHandler::ClearBrowsingDataHandler(content::WebUI* webui)
    : remover_(nullptr) {
  PrefService* prefs = Profile::FromWebUI(webui)->GetPrefs();
  clear_plugin_lso_data_enabled_.Init(prefs::kClearPluginLSODataEnabled, prefs);
  pepper_flash_settings_enabled_.Init(prefs::kPepperFlashSettingsEnabled,
                                      prefs);
  allow_deleting_browser_history_.Init(
      prefs::kAllowDeletingBrowserHistory, prefs,
      base::Bind(&ClearBrowsingDataHandler::OnBrowsingHistoryPrefChanged,
                 base::Unretained(this)));
}

ClearBrowsingDataHandler::~ClearBrowsingDataHandler() {
  if (remover_)
    remover_->RemoveObserver(this);
}

void ClearBrowsingDataHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "performClearBrowserData",
      base::Bind(&ClearBrowsingDataHandler::HandleClearBrowserData,
                 base::Unretained(this)));
}

void ClearBrowsingDataHandler::HandleClearBrowserData(
    const base::ListValue* args) {
  // We should never be called when the previous clearing has not yet finished.
  CHECK(!remover_);

  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  int site_data_mask = BrowsingDataRemover::REMOVE_SITE_DATA;
  // Don't try to clear LSO data if it's not supported.
  if (!*clear_plugin_lso_data_enabled_)
    site_data_mask &= ~BrowsingDataRemover::REMOVE_PLUGIN_DATA;

  int remove_mask = 0;
  if (*allow_deleting_browser_history_) {
    if (prefs->GetBoolean(prefs::kDeleteBrowsingHistory))
      remove_mask |= BrowsingDataRemover::REMOVE_HISTORY;
    if (prefs->GetBoolean(prefs::kDeleteDownloadHistory))
      remove_mask |= BrowsingDataRemover::REMOVE_DOWNLOADS;
  }

  if (prefs->GetBoolean(prefs::kDeleteCache))
    remove_mask |= BrowsingDataRemover::REMOVE_CACHE;

  int origin_mask = 0;
  if (prefs->GetBoolean(prefs::kDeleteCookies)) {
    remove_mask |= site_data_mask;
    origin_mask |= BrowsingDataHelper::UNPROTECTED_WEB;
  }

  if (prefs->GetBoolean(prefs::kDeletePasswords))
    remove_mask |= BrowsingDataRemover::REMOVE_PASSWORDS;

  if (prefs->GetBoolean(prefs::kDeleteFormData))
    remove_mask |= BrowsingDataRemover::REMOVE_FORM_DATA;

  // Clearing Content Licenses is only supported in Pepper Flash.
  if (prefs->GetBoolean(prefs::kDeauthorizeContentLicenses) &&
      *pepper_flash_settings_enabled_) {
    remove_mask |= BrowsingDataRemover::REMOVE_CONTENT_LICENSES;
  }

  if (prefs->GetBoolean(prefs::kDeleteHostedAppsData)) {
    remove_mask |= site_data_mask;
    origin_mask |= BrowsingDataHelper::PROTECTED_WEB;
  }

  // Record the deletion of cookies and cache.
  BrowsingDataRemover::CookieOrCacheDeletionChoice choice =
      BrowsingDataRemover::NEITHER_COOKIES_NOR_CACHE;
  if (prefs->GetBoolean(prefs::kDeleteCookies)) {
    choice = prefs->GetBoolean(prefs::kDeleteCache)
                 ? BrowsingDataRemover::BOTH_COOKIES_AND_CACHE
                 : BrowsingDataRemover::ONLY_COOKIES;
  } else if (prefs->GetBoolean(prefs::kDeleteCache)) {
    choice = BrowsingDataRemover::ONLY_CACHE;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "History.ClearBrowsingData.UserDeletedCookieOrCacheFromDialog", choice,
      BrowsingDataRemover::MAX_CHOICE_VALUE);

  // Record the circumstances under which passwords are deleted.
  if (prefs->GetBoolean(prefs::kDeletePasswords)) {
    static const char* other_types[] = {
        prefs::kDeleteBrowsingHistory,
        prefs::kDeleteDownloadHistory,
        prefs::kDeleteCache,
        prefs::kDeleteCookies,
        prefs::kDeleteFormData,
        prefs::kDeleteHostedAppsData,
        prefs::kDeauthorizeContentLicenses,
    };
    static size_t num_other_types = arraysize(other_types);
    int checked_other_types = std::count_if(
        other_types, other_types + num_other_types,
        [prefs](const std::string& pref) { return prefs->GetBoolean(pref); });
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "History.ClearBrowsingData.PasswordsDeletion.AdditionalDatatypesCount",
        checked_other_types);
  }

  int period_selected = prefs->GetInteger(prefs::kDeleteTimePeriod);
  remover_ = BrowsingDataRemoverFactory::GetForBrowserContext(profile);
  remover_->AddObserver(this);
  remover_->Remove(
      BrowsingDataRemover::Period(
          static_cast<BrowsingDataRemover::TimePeriod>(period_selected)),
      remove_mask, origin_mask);
}

void ClearBrowsingDataHandler::OnBrowsingDataRemoverDone() {
  remover_->RemoveObserver(this);
  remover_ = nullptr;
  web_ui()->CallJavascriptFunction("SettingsClearBrowserData.doneClearing");
}

void ClearBrowsingDataHandler::OnBrowsingHistoryPrefChanged() {
  web_ui()->CallJavascriptFunction(
      "SettingsClearBrowserData.setAllowDeletingHistory",
      base::FundamentalValue(*allow_deleting_browser_history_));
}

}  // namespace settings
