// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "chrome/browser/browsing_data/browsing_data_counter_factory.h"
#include "chrome/browser/browsing_data/browsing_data_counter_utils.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"

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

} // namespace

namespace settings {

// TaskObserver ----------------------------------------------------------------

class ClearBrowsingDataHandler::TaskObserver
    : public BrowsingDataRemover::Observer {
 public:
  TaskObserver(BrowsingDataRemover* remover, const base::Closure& callback);
  ~TaskObserver() override;

  void OnBrowsingDataRemoverDone() override;

 private:
  base::Closure callback_;
  ScopedObserver<BrowsingDataRemover, BrowsingDataRemover::Observer>
      remover_observer_;

  DISALLOW_COPY_AND_ASSIGN(TaskObserver);
};

ClearBrowsingDataHandler::TaskObserver::TaskObserver(
    BrowsingDataRemover* remover, const base::Closure& callback)
    : callback_(callback),
      remover_observer_(this) {
  remover_observer_.Add(remover);
}

ClearBrowsingDataHandler::TaskObserver::~TaskObserver() {}

void ClearBrowsingDataHandler::TaskObserver::OnBrowsingDataRemoverDone() {
  remover_observer_.RemoveAll();
  callback_.Run();
}

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
      "initializeClearBrowsingData",
      base::Bind(&ClearBrowsingDataHandler::HandleInitialize,
                 base::Unretained(this)));
}

void ClearBrowsingDataHandler::OnJavascriptAllowed() {
  PrefService* prefs = profile_->GetPrefs();
  profile_pref_registrar_.Init(prefs);
  profile_pref_registrar_.Add(
      prefs::kAllowDeletingBrowserHistory,
      base::Bind(&ClearBrowsingDataHandler::OnBrowsingHistoryPrefChanged,
                 base::Unretained(this)));

  if (sync_service_)
    sync_service_observer_.Add(sync_service_);

  DCHECK(counters_.empty());
  for (const std::string& pref : kCounterPrefs) {
    AddCounter(
        BrowsingDataCounterFactory::GetForProfileAndPref(profile_, pref));
  }
}

void ClearBrowsingDataHandler::OnJavascriptDisallowed() {
  profile_pref_registrar_.RemoveAll();
  sync_service_observer_.RemoveAll();
  task_observer_.reset();
  counters_.clear();
}

void ClearBrowsingDataHandler::HandleClearBrowsingData(
    const base::ListValue* args) {
  DCHECK(!task_observer_);

  PrefService* prefs = profile_->GetPrefs();

  int site_data_mask = BrowsingDataRemover::REMOVE_SITE_DATA;
  // Don't try to clear LSO data if it's not supported.
  if (!prefs->GetBoolean(prefs::kClearPluginLSODataEnabled))
    site_data_mask &= ~BrowsingDataRemover::REMOVE_PLUGIN_DATA;

  int remove_mask = 0;
  if (prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory)) {
    if (prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory))
      remove_mask |= BrowsingDataRemover::REMOVE_HISTORY;
    if (prefs->GetBoolean(browsing_data::prefs::kDeleteDownloadHistory))
      remove_mask |= BrowsingDataRemover::REMOVE_DOWNLOADS;
  }

  if (prefs->GetBoolean(browsing_data::prefs::kDeleteCache))
    remove_mask |= BrowsingDataRemover::REMOVE_CACHE;

  int origin_mask = 0;
  if (prefs->GetBoolean(browsing_data::prefs::kDeleteCookies)) {
    remove_mask |= site_data_mask;
    origin_mask |= BrowsingDataHelper::UNPROTECTED_WEB;
  }

  if (prefs->GetBoolean(browsing_data::prefs::kDeletePasswords))
    remove_mask |= BrowsingDataRemover::REMOVE_PASSWORDS;

  if (prefs->GetBoolean(browsing_data::prefs::kDeleteFormData))
    remove_mask |= BrowsingDataRemover::REMOVE_FORM_DATA;

  if (prefs->GetBoolean(browsing_data::prefs::kDeleteMediaLicenses))
    remove_mask |= BrowsingDataRemover::REMOVE_MEDIA_LICENSES;

  if (prefs->GetBoolean(browsing_data::prefs::kDeleteHostedAppsData)) {
    remove_mask |= site_data_mask;
    origin_mask |= BrowsingDataHelper::PROTECTED_WEB;
  }

  // Record the deletion of cookies and cache.
  BrowsingDataRemover::CookieOrCacheDeletionChoice choice =
      BrowsingDataRemover::NEITHER_COOKIES_NOR_CACHE;
  if (prefs->GetBoolean(browsing_data::prefs::kDeleteCookies)) {
    choice = prefs->GetBoolean(browsing_data::prefs::kDeleteCache)
                 ? BrowsingDataRemover::BOTH_COOKIES_AND_CACHE
                 : BrowsingDataRemover::ONLY_COOKIES;
  } else if (prefs->GetBoolean(browsing_data::prefs::kDeleteCache)) {
    choice = BrowsingDataRemover::ONLY_CACHE;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "History.ClearBrowsingData.UserDeletedCookieOrCacheFromDialog", choice,
      BrowsingDataRemover::MAX_CHOICE_VALUE);

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
  CHECK_EQ(1U, args->GetSize());
  CHECK(args->GetString(0, &webui_callback_id));

  BrowsingDataRemover* remover =
      BrowsingDataRemoverFactory::GetForBrowserContext(profile_);
  task_observer_ = base::MakeUnique<TaskObserver>(
      remover,
      base::Bind(&ClearBrowsingDataHandler::OnClearingTaskFinished,
                 base::Unretained(this), webui_callback_id));
  remover->RemoveAndReply(
      BrowsingDataRemover::Period(
          static_cast<browsing_data::TimePeriod>(period_selected)),
      remove_mask, origin_mask, task_observer_.get());
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

  ResolveJavascriptCallback(
      base::StringValue(webui_callback_id),
      base::FundamentalValue(show_notice));
  task_observer_.reset();
}

void ClearBrowsingDataHandler::OnBrowsingHistoryPrefChanged() {
  CallJavascriptFunction(
      "cr.webUIListenerCallback",
      base::StringValue("browsing-history-pref-changed"),
      base::FundamentalValue(
          profile_->GetPrefs()->GetBoolean(
              prefs::kAllowDeletingBrowserHistory)));
}

void ClearBrowsingDataHandler::HandleInitialize(const base::ListValue* args) {
  AllowJavascript();
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  // Needed because WebUI doesn't handle renderer crashes. See crbug.com/610450.
  task_observer_.reset();

  OnStateChanged();
  RefreshHistoryNotice();

  ResolveJavascriptCallback(
      *callback_id,
      *base::Value::CreateNullValue() /* Promise<void> */);
}

void ClearBrowsingDataHandler::OnStateChanged() {
  CallJavascriptFunction(
      "cr.webUIListenerCallback",
      base::StringValue("update-footer"),
      base::FundamentalValue(sync_service_ && sync_service_->IsSyncActive()),
      base::FundamentalValue(show_history_footer_));
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
  OnStateChanged();

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
                base::Bind(&ClearBrowsingDataHandler::UpdateCounterText,
                           base::Unretained(this)));
  counter->Restart();
  counters_.push_back(std::move(counter));
}

void ClearBrowsingDataHandler::UpdateCounterText(
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
  CallJavascriptFunction(
      "cr.webUIListenerCallback",
      base::StringValue("update-counter-text"),
      base::StringValue(result->source()->GetPrefName()),
      base::StringValue(GetChromeCounterTextFromResult(result.get())));
}

}  // namespace settings
