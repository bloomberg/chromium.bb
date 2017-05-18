// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"

#include <stddef.h>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_counter_factory.h"
#include "chrome/browser/browsing_data/browsing_data_counter_utils.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/engagement/important_sites_usage_counter.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/text/bytes_formatting.h"

using ImportantReason = ImportantSitesUtil::ImportantReason;

namespace {

const int kMaxTimesHistoryNoticeShown = 1;

// TODO(msramek): Get the list of deletion preferences from the JS side.
const char* kCounterPrefs[] = {
  browsing_data::prefs::kDeleteBrowsingHistory,
  browsing_data::prefs::kDeleteCache,
  browsing_data::prefs::kDeleteDownloadHistory,
  browsing_data::prefs::kDeleteFormData,
  browsing_data::prefs::kDeleteHostedAppsData,
  browsing_data::prefs::kDeleteMediaLicenses,
  browsing_data::prefs::kDeletePasswords,
};

const char kRegisterableDomainField[] = "registerableDomain";
const char kReasonBitField[] = "reasonBitfield";
const char kExampleOriginField[] = "exampleOrigin";
const char kIsCheckedField[] = "isChecked";
const char kStorageSizeField[] = "storageSize";
const char kHasNotificationsField[] = "hasNotifications";

} // namespace

namespace settings {

// ClearBrowsingDataHandler ----------------------------------------------------

ClearBrowsingDataHandler::ClearBrowsingDataHandler(content::WebUI* webui)
    : profile_(Profile::FromWebUI(webui)),
      sync_service_(ProfileSyncServiceFactory::GetForProfile(profile_)),
      sync_service_observer_(this),
      show_history_footer_(false),
      show_history_deletion_dialog_(false),
      weak_ptr_factory_(this) {}

ClearBrowsingDataHandler::~ClearBrowsingDataHandler() {
}

void ClearBrowsingDataHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "clearBrowsingData",
      base::Bind(&ClearBrowsingDataHandler::HandleClearBrowsingData,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getImportantSites",
      base::Bind(&ClearBrowsingDataHandler::HandleGetImportantSites,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "initializeClearBrowsingData",
      base::Bind(&ClearBrowsingDataHandler::HandleInitialize,
                 base::Unretained(this)));
}

void ClearBrowsingDataHandler::OnJavascriptAllowed() {
  if (sync_service_)
    sync_service_observer_.Add(sync_service_);

  DCHECK(counters_.empty());
  for (const std::string& pref : kCounterPrefs) {
    AddCounter(
        BrowsingDataCounterFactory::GetForProfileAndPref(profile_, pref));
  }
}

void ClearBrowsingDataHandler::OnJavascriptDisallowed() {
  sync_service_observer_.RemoveAll();
  weak_ptr_factory_.InvalidateWeakPtrs();
  counters_.clear();
}

void ClearBrowsingDataHandler::HandleClearBrowsingData(
    const base::ListValue* args) {
  PrefService* prefs = profile_->GetPrefs();

  int site_data_mask = ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA;
  // Don't try to clear LSO data if it's not supported.
  if (!prefs->GetBoolean(prefs::kClearPluginLSODataEnabled))
    site_data_mask &= ~ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA;

  int remove_mask = 0;
  if (prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory)) {
    if (prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory))
      remove_mask |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY;
    if (prefs->GetBoolean(browsing_data::prefs::kDeleteDownloadHistory))
      remove_mask |= content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS;
  }

  if (prefs->GetBoolean(browsing_data::prefs::kDeleteCache))
    remove_mask |= content::BrowsingDataRemover::DATA_TYPE_CACHE;

  int origin_mask = 0;
  if (prefs->GetBoolean(browsing_data::prefs::kDeleteCookies)) {
    remove_mask |= site_data_mask;
    origin_mask |= content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;
  }

  if (prefs->GetBoolean(browsing_data::prefs::kDeletePasswords))
    remove_mask |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS;

  if (prefs->GetBoolean(browsing_data::prefs::kDeleteFormData))
    remove_mask |= ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA;

  if (prefs->GetBoolean(browsing_data::prefs::kDeleteMediaLicenses))
    remove_mask |= content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES;

  if (prefs->GetBoolean(browsing_data::prefs::kDeleteHostedAppsData)) {
    remove_mask |= site_data_mask;
    origin_mask |= content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
  }

  // Record the deletion of cookies and cache.
  content::BrowsingDataRemover::CookieOrCacheDeletionChoice choice =
      content::BrowsingDataRemover::NEITHER_COOKIES_NOR_CACHE;
  if (prefs->GetBoolean(browsing_data::prefs::kDeleteCookies)) {
    choice = prefs->GetBoolean(browsing_data::prefs::kDeleteCache)
                 ? content::BrowsingDataRemover::BOTH_COOKIES_AND_CACHE
                 : content::BrowsingDataRemover::ONLY_COOKIES;
  } else if (prefs->GetBoolean(browsing_data::prefs::kDeleteCache)) {
    choice = content::BrowsingDataRemover::ONLY_CACHE;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "History.ClearBrowsingData.UserDeletedCookieOrCacheFromDialog", choice,
      content::BrowsingDataRemover::MAX_CHOICE_VALUE);

  // Record the circumstances under which passwords are deleted.
  if (prefs->GetBoolean(browsing_data::prefs::kDeletePasswords)) {
    static const char* other_types[] = {
        browsing_data::prefs::kDeleteBrowsingHistory,
        browsing_data::prefs::kDeleteDownloadHistory,
        browsing_data::prefs::kDeleteCache,
        browsing_data::prefs::kDeleteCookies,
        browsing_data::prefs::kDeleteFormData,
        browsing_data::prefs::kDeleteHostedAppsData,
        browsing_data::prefs::kDeleteMediaLicenses,
    };
    static size_t num_other_types = arraysize(other_types);
    int checked_other_types = std::count_if(
        other_types, other_types + num_other_types,
        [prefs](const std::string& pref) { return prefs->GetBoolean(pref); });
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "History.ClearBrowsingData.PasswordsDeletion.AdditionalDatatypesCount",
        checked_other_types);
  }

  int period_selected =
      prefs->GetInteger(browsing_data::prefs::kDeleteTimePeriod);

  std::string webui_callback_id;
  CHECK_EQ(2U, args->GetSize());
  CHECK(args->GetString(0, &webui_callback_id));

  const base::ListValue* important_sites = nullptr;
  CHECK(args->GetList(1, &important_sites));
  std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder =
      ProcessImportantSites(important_sites);

  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(profile_);

  base::OnceClosure callback =
      base::BindOnce(&ClearBrowsingDataHandler::OnClearingTaskFinished,
                     weak_ptr_factory_.GetWeakPtr(), webui_callback_id);
  browsing_data::TimePeriod time_period =
      static_cast<browsing_data::TimePeriod>(period_selected);

  browsing_data_important_sites_util::Remove(
      remove_mask, origin_mask, time_period, std::move(filter_builder), remover,
      std::move(callback));
}

std::unique_ptr<content::BrowsingDataFilterBuilder>
ClearBrowsingDataHandler::ProcessImportantSites(
    const base::ListValue* important_sites) {
  std::vector<std::string> excluding_domains;
  std::vector<int32_t> excluding_domain_reasons;
  std::vector<std::string> ignoring_domains;
  std::vector<int32_t> ignoring_domain_reasons;
  for (const auto& item : *important_sites) {
    const base::DictionaryValue* site = nullptr;
    CHECK(item.GetAsDictionary(&site));
    bool is_checked = false;
    CHECK(site->GetBoolean(kIsCheckedField, &is_checked));
    std::string domain;
    CHECK(site->GetString(kRegisterableDomainField, &domain));
    int domain_reason = -1;
    CHECK(site->GetInteger(kReasonBitField, &domain_reason));
    if (is_checked) {  // Selected important sites should be deleted.
      ignoring_domains.push_back(domain);
      ignoring_domain_reasons.push_back(domain_reason);
    } else {  // Unselected sites should be kept.
      excluding_domains.push_back(domain);
      excluding_domain_reasons.push_back(domain_reason);
    }
  }

  if (!excluding_domains.empty() || !ignoring_domains.empty()) {
    ImportantSitesUtil::RecordBlacklistedAndIgnoredImportantSites(
        profile_->GetOriginalProfile(), excluding_domains,
        excluding_domain_reasons, ignoring_domains, ignoring_domain_reasons);
  }

  std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder(
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::BLACKLIST));
  for (const std::string& domain : excluding_domains) {
    filter_builder->AddRegisterableDomain(domain);
  }
  return filter_builder;
}

void ClearBrowsingDataHandler::OnClearingTaskFinished(
    const std::string& webui_callback_id) {
  PrefService* prefs = profile_->GetPrefs();
  int notice_shown_times = prefs->GetInteger(
      browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes);

  // When the deletion is complete, we might show an additional dialog with
  // a notice about other forms of browsing history. This is the case if
  const bool show_notice =
      // 1. The dialog is relevant for the user.
      show_history_deletion_dialog_ &&
      // 2. The notice has been shown less than |kMaxTimesHistoryNoticeShown|.
      notice_shown_times < kMaxTimesHistoryNoticeShown &&
      // 3. The selected data types contained browsing history.
      prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory);

  if (show_notice) {
    // Increment the preference.
    prefs->SetInteger(
        browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes,
        notice_shown_times + 1);
  }

  UMA_HISTOGRAM_BOOLEAN(
      "History.ClearBrowsingData.ShownHistoryNoticeAfterClearing", show_notice);

  ResolveJavascriptCallback(base::Value(webui_callback_id),
                            base::Value(show_notice));
}

void ClearBrowsingDataHandler::HandleGetImportantSites(
    const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  DCHECK(base::FeatureList::IsEnabled(features::kImportantSitesInCbd));

  Profile* profile = profile_->GetOriginalProfile();
  bool important_sites_dialog_disabled =
      ImportantSitesUtil::IsDialogDisabled(profile);

  if (important_sites_dialog_disabled) {
    ResolveJavascriptCallback(base::Value(callback_id), base::ListValue());
    return;
  }

  std::vector<ImportantSitesUtil::ImportantDomainInfo> important_sites =
      ImportantSitesUtil::GetImportantRegisterableDomains(
          profile, ImportantSitesUtil::kMaxImportantSites);
  content::StoragePartition* partition =
      content::BrowserContext::GetDefaultStoragePartition(profile);
  storage::QuotaManager* quota_manager = partition->GetQuotaManager();
  content::DOMStorageContext* dom_storage = partition->GetDOMStorageContext();

  ImportantSitesUsageCounter::GetUsage(
      std::move(important_sites), quota_manager, dom_storage,
      base::BindOnce(&ClearBrowsingDataHandler::OnFetchImportantSitesFinished,
                     weak_ptr_factory_.GetWeakPtr(), callback_id));
}

void ClearBrowsingDataHandler::OnFetchImportantSitesFinished(
    const std::string& callback_id,
    std::vector<ImportantSitesUtil::ImportantDomainInfo> important_sites) {
  base::ListValue important_sites_list;

  for (const auto& info : important_sites) {
    auto entry = base::MakeUnique<base::DictionaryValue>();
    entry->SetString(kRegisterableDomainField, info.registerable_domain);
    // The |reason_bitfield| is only passed to Javascript to be logged
    // from |HandleClearBrowsingData|.
    entry->SetInteger(kReasonBitField, info.reason_bitfield);
    entry->SetString(kExampleOriginField, info.example_origin.spec());
    // Initially all sites are selected for deletion.
    entry->SetBoolean(kIsCheckedField, true);
    entry->SetString(kStorageSizeField, ui::FormatBytes(info.usage));
    bool has_notifications =
        (info.reason_bitfield & (1 << ImportantReason::NOTIFICATIONS)) != 0;
    entry->SetBoolean(kHasNotificationsField, has_notifications);
    important_sites_list.Append(std::move(entry));
  }
  ResolveJavascriptCallback(base::Value(callback_id), important_sites_list);
}

void ClearBrowsingDataHandler::HandleInitialize(const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  // Needed because WebUI doesn't handle renderer crashes. See crbug.com/610450.
  weak_ptr_factory_.InvalidateWeakPtrs();

  UpdateSyncState();
  RefreshHistoryNotice();

  // Restart the counters each time the dialog is reopened.
  for (const auto& counter : counters_)
    counter->Restart();

  ResolveJavascriptCallback(*callback_id, base::Value() /* Promise<void> */);
}

void ClearBrowsingDataHandler::OnStateChanged(syncer::SyncService* sync) {
  UpdateSyncState();
}

void ClearBrowsingDataHandler::UpdateSyncState() {
  CallJavascriptFunction(
      "cr.webUIListenerCallback", base::Value("update-footer"),
      base::Value(sync_service_ && sync_service_->IsSyncActive()),
      base::Value(show_history_footer_));
}

void ClearBrowsingDataHandler::RefreshHistoryNotice() {
  browsing_data::ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      sync_service_,
      WebHistoryServiceFactory::GetForProfile(profile_),
      base::Bind(&ClearBrowsingDataHandler::UpdateHistoryNotice,
                 weak_ptr_factory_.GetWeakPtr()));

  // If the dialog with history notice has been shown less than
  // |kMaxTimesHistoryNoticeShown| times, we might have to show it when the
  // user deletes history. Find out if the conditions are met.
  int notice_shown_times = profile_->GetPrefs()->GetInteger(
      browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes);

  if (notice_shown_times < kMaxTimesHistoryNoticeShown) {
    browsing_data::ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
        sync_service_,
        WebHistoryServiceFactory::GetForProfile(profile_),
        chrome::GetChannel(),
        base::Bind(&ClearBrowsingDataHandler::UpdateHistoryDeletionDialog,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void ClearBrowsingDataHandler::UpdateHistoryNotice(bool show) {
  show_history_footer_ = show;
  UpdateSyncState();

  UMA_HISTOGRAM_BOOLEAN(
      "History.ClearBrowsingData.HistoryNoticeShownInFooterWhenUpdated",
      show_history_footer_);
}

void ClearBrowsingDataHandler::UpdateHistoryDeletionDialog(bool show) {
  // This is used by OnClearingTaskFinished (when the deletion finishes).
  show_history_deletion_dialog_ = show;
}

void ClearBrowsingDataHandler::AddCounter(
    std::unique_ptr<browsing_data::BrowsingDataCounter> counter) {
  counter->Init(profile_->GetPrefs(),
                browsing_data::ClearBrowsingDataTab::ADVANCED,
                base::Bind(&ClearBrowsingDataHandler::UpdateCounterText,
                           base::Unretained(this)));
  counters_.push_back(std::move(counter));
}

void ClearBrowsingDataHandler::UpdateCounterText(
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
  CallJavascriptFunction(
      "cr.webUIListenerCallback", base::Value("update-counter-text"),
      base::Value(result->source()->GetPrefName()),
      base::Value(GetChromeCounterTextFromResult(result.get())));
}

}  // namespace settings
