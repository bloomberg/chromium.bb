// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/clear_browser_data_handler.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_counter_factory.h"
#include "chrome/browser/browsing_data/browsing_data_counter_utils.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/google/core/browser/google_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

const char kClearBrowsingDataLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=settings_clear_browsing_data";

const char kMyActivityUrlInFooter[] =
    "https://history.google.com/history/?utm_source=chrome_cbd";

const char kMyActivityUrlInDialog[] =
    "https://history.google.com/history/?utm_source=chrome_n";

const int kMaxTimesHistoryNoticeShown = 1;

const char* kCounterPrefs[] = {
  browsing_data::prefs::kDeleteBrowsingHistory,
  browsing_data::prefs::kDeleteCache,
  browsing_data::prefs::kDeleteFormData,
  browsing_data::prefs::kDeleteMediaLicenses,
  browsing_data::prefs::kDeletePasswords,
};

}  // namespace

namespace options {

ClearBrowserDataHandler::ClearBrowserDataHandler()
    : remover_(nullptr),
      sync_service_(nullptr),
      should_show_history_notice_(false),
      should_show_history_deletion_dialog_(false),
      weak_ptr_factory_(this) {
}

ClearBrowserDataHandler::~ClearBrowserDataHandler() {
  if (remover_)
    remover_->RemoveObserver(this);
  if (sync_service_)
    sync_service_->RemoveObserver(this);
}

void ClearBrowserDataHandler::InitializeHandler() {
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  clear_plugin_lso_data_enabled_.Init(prefs::kClearPluginLSODataEnabled, prefs);
  allow_deleting_browser_history_.Init(
      prefs::kAllowDeletingBrowserHistory,
      prefs,
      base::Bind(&ClearBrowserDataHandler::OnBrowsingHistoryPrefChanged,
                 base::Unretained(this)));

  if (AreCountersEnabled()) {
    Profile* profile = Profile::FromWebUI(web_ui());
    for (const std::string& pref : kCounterPrefs) {
      AddCounter(
          BrowsingDataCounterFactory::GetForProfileAndPref(profile, pref));
    }

    sync_service_ = ProfileSyncServiceFactory::GetForProfile(profile);
    if (sync_service_)
      sync_service_->AddObserver(this);
  }
}

void ClearBrowserDataHandler::InitializePage() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "ClearBrowserDataOverlay.createFooter", base::Value(AreCountersEnabled()),
      base::Value(sync_service_ && sync_service_->IsSyncActive()),
      base::Value(should_show_history_notice_));
  RefreshHistoryNotice();
  UpdateInfoBannerVisibility();
  OnBrowsingHistoryPrefChanged();
  bool removal_in_progress = !!remover_;
  web_ui()->CallJavascriptFunctionUnsafe("ClearBrowserDataOverlay.setClearing",
                                         base::Value(removal_in_progress));

  web_ui()->CallJavascriptFunctionUnsafe(
      "ClearBrowserDataOverlay.markInitializationComplete");
}

void ClearBrowserDataHandler::UpdateInfoBannerVisibility() {
  base::string16 text;

  Profile* profile = Profile::FromWebUI(web_ui());
  auto availability = IncognitoModePrefs::GetAvailability(profile->GetPrefs());
  if (availability == IncognitoModePrefs::ENABLED) {
    base::Time last_clear_time = base::Time::FromInternalValue(
        profile->GetPrefs()->GetInt64(
            browsing_data::prefs::kLastClearBrowsingDataTime));

    const base::TimeDelta since_clear = base::Time::Now() - last_clear_time;
    if (since_clear < base::TimeDelta::FromHours(base::Time::kHoursPerDay)) {
      ui::Accelerator acc = chrome::GetPrimaryChromeAcceleratorForCommandId(
          IDC_NEW_INCOGNITO_WINDOW);
      DCHECK_NE(ui::VKEY_UNKNOWN, acc.key_code());
      text = l10n_util::GetStringFUTF16(IDS_CLEAR_BROWSING_DATA_INFO_BAR_TEXT,
                                        acc.GetShortcutText());
    }
  }

  web_ui()->CallJavascriptFunctionUnsafe(
      "ClearBrowserDataOverlay.setBannerText", base::StringValue(text));
}

void ClearBrowserDataHandler::OnPageOpened(const base::ListValue* value) {
  for (const auto& counter : counters_) {
    DCHECK(AreCountersEnabled());
    counter->Restart();
  }
}

void ClearBrowserDataHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
      {"clearBrowserDataLabel", IDS_CLEAR_BROWSING_DATA_LABEL},
      {"clearBrowserDataSyncWarning", IDS_CLEAR_BROWSING_DATA_SYNCED_DELETION},
      {"clearBrowserDataSupportString",
       AreCountersEnabled() ? IDS_CLEAR_BROWSING_DATA_SOME_STUFF_REMAINS_SIMPLE
                            : IDS_CLEAR_BROWSING_DATA_SOME_STUFF_REMAINS},
      {"clearBrowserDataHistoryNoticeTitle",
       IDS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_TITLE},
      {"clearBrowserDataHistoryNoticeOk",
       IDS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_OK},
      {"deleteBrowsingHistoryCheckbox", IDS_DEL_BROWSING_HISTORY_CHKBOX},
      {"deleteDownloadHistoryCheckbox", IDS_DEL_DOWNLOAD_HISTORY_CHKBOX},
      {"deleteCacheCheckbox", IDS_DEL_CACHE_CHKBOX},
      {"deleteCookiesCheckbox", IDS_DEL_COOKIES_CHKBOX},
      {"deleteCookiesFlashCheckbox", IDS_DEL_COOKIES_FLASH_CHKBOX},
      {"deletePasswordsCheckbox", IDS_DEL_PASSWORDS_CHKBOX},
      {"deleteFormDataCheckbox", IDS_DEL_FORM_DATA_CHKBOX},
      {"deleteHostedAppsDataCheckbox", IDS_DEL_HOSTED_APPS_DATA_CHKBOX},
      {"deleteMediaLicensesCheckbox", IDS_DEL_MEDIA_LICENSES_CHKBOX},
      {"clearBrowserDataCommit", IDS_CLEAR_BROWSING_DATA_COMMIT},
      {"flashStorageUrl", IDS_FLASH_STORAGE_URL},
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "clearBrowserDataOverlay",
                IDS_CLEAR_BROWSING_DATA_TITLE);
  localized_strings->SetString("clearBrowsingDataLearnMoreUrl",
                               kClearBrowsingDataLearnMoreUrl);
  localized_strings->SetString(
      "clearBrowserDataHistoryFooter",
      l10n_util::GetStringFUTF16(
          IDS_CLEAR_BROWSING_DATA_HISTORY_FOOTER,
          base::ASCIIToUTF16(kMyActivityUrlInFooter)));
  localized_strings->SetString(
      "clearBrowserDataHistoryNotice",
      l10n_util::GetStringFUTF16(
          IDS_CLEAR_BROWSING_DATA_HISTORY_NOTICE,
          base::ASCIIToUTF16(kMyActivityUrlInDialog)));

  base::ListValue* time_list = new base::ListValue;
  for (int i = 0; i < 5; i++) {
    base::string16 label_string;
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
    std::unique_ptr<base::ListValue> option(new base::ListValue());
    option->AppendInteger(i);
    option->AppendString(label_string);
    time_list->Append(std::move(option));
  }
  localized_strings->Set("clearBrowserDataTimeList", time_list);
  localized_strings->SetBoolean("showDeleteBrowsingHistoryCheckboxes",
                                !Profile::FromWebUI(web_ui())->IsSupervised());
}

void ClearBrowserDataHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  web_ui()->RegisterMessageCallback("performClearBrowserData",
      base::Bind(&ClearBrowserDataHandler::HandleClearBrowserData,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openedClearBrowserData",
      base::Bind(&ClearBrowserDataHandler::OnPageOpened,
                 base::Unretained(this)));
}

void ClearBrowserDataHandler::HandleClearBrowserData(
    const base::ListValue* value) {
  // We should never be called when the previous clearing has not yet finished.
  CHECK(!remover_);

  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  int site_data_mask = BrowsingDataRemover::REMOVE_SITE_DATA;
  // Don't try to clear LSO data if it's not supported.
  if (!*clear_plugin_lso_data_enabled_)
    site_data_mask &= ~BrowsingDataRemover::REMOVE_PLUGIN_DATA;

  int remove_mask = 0;
  int origin_mask = 0;
  if (prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory) &&
      *allow_deleting_browser_history_) {
    remove_mask |= BrowsingDataRemover::REMOVE_HISTORY;
  }
  if (prefs->GetBoolean(browsing_data::prefs::kDeleteDownloadHistory) &&
      *allow_deleting_browser_history_) {
    remove_mask |= BrowsingDataRemover::REMOVE_DOWNLOADS;
  }
  if (prefs->GetBoolean(browsing_data::prefs::kDeleteCache))
    remove_mask |= BrowsingDataRemover::REMOVE_CACHE;
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
      "History.ClearBrowsingData.UserDeletedCookieOrCacheFromDialog",
      choice, BrowsingDataRemover::MAX_CHOICE_VALUE);

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
        other_types,
        other_types + num_other_types,
        [prefs](const std::string& pref) { return prefs->GetBoolean(pref); });
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "History.ClearBrowsingData.PasswordsDeletion.AdditionalDatatypesCount",
        checked_other_types);
  }

  remover_ = BrowsingDataRemoverFactory::GetForBrowserContext(profile);
  remover_->AddObserver(this);
  int period_selected =
      prefs->GetInteger(browsing_data::prefs::kDeleteTimePeriod);
  browsing_data::TimePeriod time_period =
      static_cast<browsing_data::TimePeriod>(period_selected);
  browsing_data::RecordDeletionForPeriod(time_period);
  remover_->RemoveAndReply(
      browsing_data::CalculateBeginDeleteTime(time_period),
      browsing_data::CalculateEndDeleteTime(time_period),
      remove_mask, origin_mask, this);

  // Store the clear browsing data time. Next time the clear browsing data
  // dialog is open, this time is used to decide whether to display an info
  // banner or not.
  prefs->SetInt64(browsing_data::prefs::kLastClearBrowsingDataTime,
                  base::Time::Now().ToInternalValue());
}

void ClearBrowserDataHandler::OnBrowsingDataRemoverDone() {
  remover_->RemoveObserver(this);
  remover_ = nullptr;

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  int notice_shown_times = prefs->GetInteger(
      browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes);

  // When the deletion is complete, we might show an additional dialog with
  // a notice about other forms of browsing history. This is the case if
  const bool show_notice =
      // 1. The dialog is relevant for the user.
      should_show_history_deletion_dialog_ &&
      // 2. The selected data types contained browsing history.
      prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory) &&
      // 3. The notice has been shown less than |kMaxTimesHistoryNoticeShown|.
      notice_shown_times < kMaxTimesHistoryNoticeShown;

  if (show_notice) {
    // Increment the preference.
    prefs->SetInteger(
        browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes,
        notice_shown_times + 1);
  }

  UMA_HISTOGRAM_BOOLEAN(
      "History.ClearBrowsingData.ShownHistoryNoticeAfterClearing", show_notice);

  web_ui()->CallJavascriptFunctionUnsafe("ClearBrowserDataOverlay.doneClearing",
                                         base::Value(show_notice));
}

void ClearBrowserDataHandler::OnBrowsingHistoryPrefChanged() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "ClearBrowserDataOverlay.setAllowDeletingHistory",
      base::Value(*allow_deleting_browser_history_));
}

void ClearBrowserDataHandler::AddCounter(
    std::unique_ptr<browsing_data::BrowsingDataCounter> counter) {
  DCHECK(AreCountersEnabled());

  counter->Init(Profile::FromWebUI(web_ui())->GetPrefs(),
                base::Bind(&ClearBrowserDataHandler::UpdateCounterText,
                           base::Unretained(this)));
  counters_.push_back(std::move(counter));
}

void ClearBrowserDataHandler::UpdateCounterText(
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
  DCHECK(AreCountersEnabled());
  web_ui()->CallJavascriptFunctionUnsafe(
      "ClearBrowserDataOverlay.updateCounter",
      base::StringValue(result->source()->GetPrefName()),
      base::StringValue(GetChromeCounterTextFromResult(result.get())));
}

void ClearBrowserDataHandler::OnStateChanged(syncer::SyncService* sync) {
  UpdateSyncState();
}

void ClearBrowserDataHandler::UpdateSyncState() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "ClearBrowserDataOverlay.updateSyncWarningAndHistoryFooter",
      base::Value(sync_service_ && sync_service_->IsSyncActive()),
      base::Value(should_show_history_notice_));
}

void ClearBrowserDataHandler::RefreshHistoryNotice() {
  browsing_data::ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      sync_service_,
      WebHistoryServiceFactory::GetForProfile(Profile::FromWebUI(web_ui())),
      base::Bind(&ClearBrowserDataHandler::UpdateHistoryNotice,
                 weak_ptr_factory_.GetWeakPtr()));

  // If the dialog with history notice has been shown less than
  // |kMaxTimesHistoryNoticeShown| times, we might have to show it when the
  // user deletes history. Find out if the conditions are met.
  int notice_shown_times = Profile::FromWebUI(web_ui())->GetPrefs()->GetInteger(
      browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes);

  if (notice_shown_times < kMaxTimesHistoryNoticeShown) {
    browsing_data::ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
        sync_service_,
        WebHistoryServiceFactory::GetForProfile(Profile::FromWebUI(web_ui())),
        chrome::GetChannel(),
        base::Bind(&ClearBrowserDataHandler::UpdateHistoryDeletionDialog,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void ClearBrowserDataHandler::UpdateHistoryNotice(bool show) {
  should_show_history_notice_ = show;
  UpdateSyncState();

  UMA_HISTOGRAM_BOOLEAN(
      "History.ClearBrowsingData.HistoryNoticeShownInFooterWhenUpdated",
      should_show_history_notice_);
}

void ClearBrowserDataHandler::UpdateHistoryDeletionDialog(bool show) {
  // This is used by OnBrowsingDataRemoverDone (when the deletion finishes).
  should_show_history_deletion_dialog_ = show;
}

}  // namespace options
