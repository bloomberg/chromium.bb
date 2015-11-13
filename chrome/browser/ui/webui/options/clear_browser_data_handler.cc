// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/clear_browser_data_handler.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/autofill_counter.h"
#include "chrome/browser/browsing_data/browsing_data_counter.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/browsing_data/cache_counter.h"
#include "chrome/browser/browsing_data/history_counter.h"
#include "chrome/browser/browsing_data/passwords_counter.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/google/core/browser/google_util.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

const char kClearBrowsingDataLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=settings_clear_browsing_data";

bool AreCountersEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableClearBrowsingDataCounters)) {
    return true;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableClearBrowsingDataCounters)) {
    return false;
  }

  // Enabled by default.
  return true;
}

// A helper function to display the size of cache in units of MB or higher.
// We need this, as 1 MB is the lowest nonzero cache size displayed by the
// counter.
base::string16 FormatBytesMBOrHigher(BrowsingDataCounter::ResultInt bytes) {
  if (ui::GetByteDisplayUnits(bytes) >= ui::DataUnits::DATA_UNITS_MEBIBYTE)
    return ui::FormatBytes(bytes);

  return ui::FormatBytesWithUnits(
      bytes, ui::DataUnits::DATA_UNITS_MEBIBYTE, true);
}

}  // namespace

namespace options {

ClearBrowserDataHandler::ClearBrowserDataHandler()
    : remover_(nullptr),
      sync_service_(nullptr) {
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
  pepper_flash_settings_enabled_.Init(prefs::kPepperFlashSettingsEnabled,
                                      prefs);
  allow_deleting_browser_history_.Init(
      prefs::kAllowDeletingBrowserHistory,
      prefs,
      base::Bind(&ClearBrowserDataHandler::OnBrowsingHistoryPrefChanged,
                 base::Unretained(this)));

  if (AreCountersEnabled()) {
    AddCounter(make_scoped_ptr(new PasswordsCounter()));
    AddCounter(make_scoped_ptr(new HistoryCounter()));
    AddCounter(make_scoped_ptr(new CacheCounter()));
    AddCounter(make_scoped_ptr(new AutofillCounter()));

    sync_service_ =
        ProfileSyncServiceFactory::GetForProfile(Profile::FromWebUI(web_ui()));
    if (sync_service_)
      sync_service_->AddObserver(this);
  }
}

void ClearBrowserDataHandler::InitializePage() {
  web_ui()->CallJavascriptFunction(
      "ClearBrowserDataOverlay.createFooter",
      base::FundamentalValue(AreCountersEnabled()),
      base::FundamentalValue(sync_service_ && sync_service_->IsSyncActive()));
  UpdateInfoBannerVisibility();
  OnBrowsingHistoryPrefChanged();
  bool removal_in_progress = !!remover_;
  web_ui()->CallJavascriptFunction("ClearBrowserDataOverlay.setClearing",
                                   base::FundamentalValue(removal_in_progress));

  web_ui()->CallJavascriptFunction(
      "ClearBrowserDataOverlay.markInitializationComplete");
}

void ClearBrowserDataHandler::UpdateInfoBannerVisibility() {
  base::string16 text;

  Profile* profile = Profile::FromWebUI(web_ui());
  auto availability = IncognitoModePrefs::GetAvailability(profile->GetPrefs());
  if (availability == IncognitoModePrefs::ENABLED) {
    base::Time last_clear_time = base::Time::FromInternalValue(
        profile->GetPrefs()->GetInt64(prefs::kLastClearBrowsingDataTime));

    const base::TimeDelta since_clear = base::Time::Now() - last_clear_time;
    if (since_clear < base::TimeDelta::FromHours(base::Time::kHoursPerDay)) {
      ui::Accelerator acc = chrome::GetPrimaryChromeAcceleratorForCommandId(
          IDC_NEW_INCOGNITO_WINDOW);
      DCHECK_NE(ui::VKEY_UNKNOWN, acc.key_code());
      text = l10n_util::GetStringFUTF16(IDS_CLEAR_BROWSING_DATA_INFO_BAR_TEXT,
                                        acc.GetShortcutText());
    }
  }

  web_ui()->CallJavascriptFunction("ClearBrowserDataOverlay.setBannerText",
                                   base::StringValue(text));
}

// static
base::string16 ClearBrowserDataHandler::GetCounterTextFromResult(
    const BrowsingDataCounter::Result* result) {
  base::string16 text;
  std::string pref_name = result->source()->GetPrefName();

  if (!result->Finished()) {
    // The counter is still counting.
    text = l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_CALCULATING);

  } else if (pref_name == prefs::kDeletePasswords) {
    // Passwords counter.
    BrowsingDataCounter::ResultInt passwords_count =
        static_cast<const BrowsingDataCounter::FinishedResult*>(
            result)->Value();
    text = l10n_util::GetPluralStringFUTF16(
        IDS_DEL_PASSWORDS_COUNTER, passwords_count);

  } else if (pref_name == prefs::kDeleteCache) {
    // Cache counter.
    BrowsingDataCounter::ResultInt cache_size_bytes =
        static_cast<const BrowsingDataCounter::FinishedResult*>(
            result)->Value();

    PrefService* prefs = result->source()->GetProfile()->GetPrefs();
    BrowsingDataRemover::TimePeriod time_period =
        static_cast<BrowsingDataRemover::TimePeriod>(
            prefs->GetInteger(prefs::kDeleteTimePeriod));

    // Three cases: Nonzero result for the entire cache, nonzero result for
    // a subset of cache (i.e. a finite time interval), and almost zero (< 1MB).
    static const int kBytesInAMegabyte = 1024 * 1024;
    if (cache_size_bytes >= kBytesInAMegabyte) {
      base::string16 formatted_size = FormatBytesMBOrHigher(cache_size_bytes);
      text = time_period == BrowsingDataRemover::EVERYTHING
          ? formatted_size
          : l10n_util::GetStringFUTF16(IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
                                       formatted_size);
    } else {
      text = l10n_util::GetStringUTF16(IDS_DEL_CACHE_COUNTER_ALMOST_EMPTY);
    }

  } else if (pref_name == prefs::kDeleteBrowsingHistory) {
    // History counter.
    const HistoryCounter::HistoryResult* history_result =
        static_cast<const HistoryCounter::HistoryResult*>(result);
    BrowsingDataCounter::ResultInt local_item_count = history_result->Value();
    bool has_synced_visits = history_result->has_synced_visits();

    text = has_synced_visits
        ? l10n_util::GetPluralStringFUTF16(
              IDS_DEL_BROWSING_HISTORY_COUNTER_SYNCED, local_item_count)
        : l10n_util::GetPluralStringFUTF16(
              IDS_DEL_BROWSING_HISTORY_COUNTER, local_item_count);

  } else if (pref_name == prefs::kDeleteFormData) {
    // Autofill counter.
    const AutofillCounter::AutofillResult* autofill_result =
        static_cast<const AutofillCounter::AutofillResult*>(result);
    AutofillCounter::ResultInt num_suggestions = autofill_result->Value();
    AutofillCounter::ResultInt num_credit_cards =
        autofill_result->num_credit_cards();
    AutofillCounter::ResultInt num_addresses = autofill_result->num_addresses();

    std::vector<base::string16> displayed_strings;

    if (num_credit_cards) {
      displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
          IDS_DEL_AUTOFILL_COUNTER_CREDIT_CARDS, num_credit_cards));
    }
    if (num_addresses) {
      displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
          IDS_DEL_AUTOFILL_COUNTER_ADDRESSES, num_addresses));
    }
    if (num_suggestions) {
      // We use a different wording for autocomplete suggestions based on the
      // length of the entire string.
      switch (displayed_strings.size()) {
        case 0:
          displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
              IDS_DEL_AUTOFILL_COUNTER_SUGGESTIONS, num_suggestions));
          break;
        case 1:
          displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
              IDS_DEL_AUTOFILL_COUNTER_SUGGESTIONS_LONG, num_suggestions));
          break;
        case 2:
          displayed_strings.push_back(l10n_util::GetPluralStringFUTF16(
              IDS_DEL_AUTOFILL_COUNTER_SUGGESTIONS_SHORT, num_suggestions));
          break;
        default:
          NOTREACHED();
      }
    }

    // Construct the resulting string from the sections in |displayed_strings|.
    switch (displayed_strings.size()) {
      case 0:
        text = l10n_util::GetStringUTF16(IDS_DEL_AUTOFILL_COUNTER_EMPTY);
        break;
      case 1:
        text = displayed_strings[0];
        break;
      case 2:
        text = l10n_util::GetStringFUTF16(IDS_DEL_AUTOFILL_COUNTER_TWO_TYPES,
                                          displayed_strings[0],
                                          displayed_strings[1]);
        break;
      case 3:
        text = l10n_util::GetStringFUTF16(IDS_DEL_AUTOFILL_COUNTER_THREE_TYPES,
                                          displayed_strings[0],
                                          displayed_strings[1],
                                          displayed_strings[2]);
        break;
      default:
        NOTREACHED();
    }
  }

  return text;
}

void ClearBrowserDataHandler::OnPageOpened(const base::ListValue* value) {
  for (BrowsingDataCounter* counter : counters_) {
    DCHECK(AreCountersEnabled());
    counter->Restart();
  }
}

void ClearBrowserDataHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "clearBrowserDataLabel", IDS_CLEAR_BROWSING_DATA_LABEL },
    { "clearBrowserDataSyncWarning", IDS_CLEAR_BROWSING_DATA_SYNCED_DELETION },
    { "clearBrowserDataSupportString", AreCountersEnabled()
        ? IDS_CLEAR_BROWSING_DATA_SOME_STUFF_REMAINS_SIMPLE
        : IDS_CLEAR_BROWSING_DATA_SOME_STUFF_REMAINS },
    { "deleteBrowsingHistoryCheckbox", IDS_DEL_BROWSING_HISTORY_CHKBOX },
    { "deleteDownloadHistoryCheckbox", IDS_DEL_DOWNLOAD_HISTORY_CHKBOX },
    { "deleteCacheCheckbox", IDS_DEL_CACHE_CHKBOX },
    { "deleteCookiesCheckbox", IDS_DEL_COOKIES_CHKBOX },
    { "deleteCookiesFlashCheckbox", IDS_DEL_COOKIES_FLASH_CHKBOX },
    { "deletePasswordsCheckbox", IDS_DEL_PASSWORDS_CHKBOX },
    { "deleteFormDataCheckbox", IDS_DEL_FORM_DATA_CHKBOX },
    { "deleteHostedAppsDataCheckbox", IDS_DEL_HOSTED_APPS_DATA_CHKBOX },
    { "deauthorizeContentLicensesCheckbox",
      IDS_DEAUTHORIZE_CONTENT_LICENSES_CHKBOX },
    { "clearBrowserDataCommit", IDS_CLEAR_BROWSING_DATA_COMMIT },
    { "flashStorageUrl", IDS_FLASH_STORAGE_URL },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "clearBrowserDataOverlay",
                IDS_CLEAR_BROWSING_DATA_TITLE);
  localized_strings->SetString("clearBrowsingDataLearnMoreUrl",
                               kClearBrowsingDataLearnMoreUrl);

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
    base::ListValue* option = new base::ListValue();
    option->Append(new base::FundamentalValue(i));
    option->Append(new base::StringValue(label_string));
    time_list->Append(option);
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
  if (prefs->GetBoolean(prefs::kDeleteBrowsingHistory) &&
      *allow_deleting_browser_history_) {
    remove_mask |= BrowsingDataRemover::REMOVE_HISTORY;
  }
  if (prefs->GetBoolean(prefs::kDeleteDownloadHistory) &&
      *allow_deleting_browser_history_) {
    remove_mask |= BrowsingDataRemover::REMOVE_DOWNLOADS;
  }
  if (prefs->GetBoolean(prefs::kDeleteCache))
    remove_mask |= BrowsingDataRemover::REMOVE_CACHE;
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
      "History.ClearBrowsingData.UserDeletedCookieOrCacheFromDialog",
      choice, BrowsingDataRemover::MAX_CHOICE_VALUE);

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
        other_types,
        other_types + num_other_types,
        [prefs](const std::string& pref) { return prefs->GetBoolean(pref); });
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "History.ClearBrowsingData.PasswordsDeletion.AdditionalDatatypesCount",
        checked_other_types);
  }

  // BrowsingDataRemover deletes itself when done.
  int period_selected = prefs->GetInteger(prefs::kDeleteTimePeriod);
  remover_ = BrowsingDataRemover::CreateForPeriod(profile,
      static_cast<BrowsingDataRemover::TimePeriod>(period_selected));
  remover_->AddObserver(this);
  remover_->Remove(remove_mask, origin_mask);

  // Store the clear browsing data time. Next time the clear browsing data
  // dialog is open, this time is used to decide whether to display an info
  // banner or not.
  prefs->SetInt64(prefs::kLastClearBrowsingDataTime,
                  base::Time::Now().ToInternalValue());
}

void ClearBrowserDataHandler::OnBrowsingDataRemoverDone() {
  // No need to remove ourselves as an observer as BrowsingDataRemover deletes
  // itself after we return.
  remover_ = NULL;
  web_ui()->CallJavascriptFunction("ClearBrowserDataOverlay.doneClearing");
}

void ClearBrowserDataHandler::OnBrowsingHistoryPrefChanged() {
  web_ui()->CallJavascriptFunction(
      "ClearBrowserDataOverlay.setAllowDeletingHistory",
      base::FundamentalValue(*allow_deleting_browser_history_));
}

void ClearBrowserDataHandler::AddCounter(
    scoped_ptr<BrowsingDataCounter> counter) {
  DCHECK(AreCountersEnabled());

  counter->Init(
      Profile::FromWebUI(web_ui()),
      base::Bind(&ClearBrowserDataHandler::UpdateCounterText,
                 base::Unretained(this)));
  counters_.push_back(counter.Pass());
}

void ClearBrowserDataHandler::UpdateCounterText(
    scoped_ptr<BrowsingDataCounter::Result> result) {
  DCHECK(AreCountersEnabled());
  web_ui()->CallJavascriptFunction(
      "ClearBrowserDataOverlay.updateCounter",
      base::StringValue(result->source()->GetPrefName()),
      base::StringValue(GetCounterTextFromResult(result.get())));
}

void ClearBrowserDataHandler::OnStateChanged() {
  web_ui()->CallJavascriptFunction(
      "ClearBrowserDataOverlay.updateSyncWarning",
      base::FundamentalValue(sync_service_ && sync_service_->IsSyncActive()));
}

}  // namespace options
