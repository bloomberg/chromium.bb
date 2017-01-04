// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_counter_utils.h"

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/cache_counter.h"
#include "chrome/browser/browsing_data/media_licenses_counter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/features/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browsing_data/hosted_apps_counter.h"
#endif

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
base::string16 FormatBytesMBOrHigher(
    browsing_data::BrowsingDataCounter::ResultInt bytes) {
  if (ui::GetByteDisplayUnits(bytes) >= ui::DataUnits::DATA_UNITS_MEBIBYTE)
    return ui::FormatBytes(bytes);

  return ui::FormatBytesWithUnits(
      bytes, ui::DataUnits::DATA_UNITS_MEBIBYTE, true);
}

base::string16 GetChromeCounterTextFromResult(
    const browsing_data::BrowsingDataCounter::Result* result) {
  std::string pref_name = result->source()->GetPrefName();

  if (!result->Finished()) {
    // The counter is still counting.
    return l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_CALCULATING);
  }

  if (pref_name == browsing_data::prefs::kDeleteCache) {
    // Cache counter.
    const auto* cache_result =
        static_cast<const CacheCounter::CacheResult*>(result);
    int64_t cache_size_bytes = cache_result->cache_size();
    bool is_upper_limit = cache_result->is_upper_limit();

    // Three cases: Nonzero result for the entire cache, nonzero result for
    // a subset of cache (i.e. a finite time interval), and almost zero (< 1MB).
    static const int kBytesInAMegabyte = 1024 * 1024;
    if (cache_size_bytes >= kBytesInAMegabyte) {
      base::string16 formatted_size = FormatBytesMBOrHigher(cache_size_bytes);
      return !is_upper_limit
                 ? formatted_size
                 : l10n_util::GetStringFUTF16(
                       IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE, formatted_size);
    }
    return l10n_util::GetStringUTF16(IDS_DEL_CACHE_COUNTER_ALMOST_EMPTY);
  }

  if (pref_name == browsing_data::prefs::kDeleteMediaLicenses) {
    const MediaLicensesCounter::MediaLicenseResult* media_license_result =
        static_cast<const MediaLicensesCounter::MediaLicenseResult*>(result);
    if (media_license_result->Value() > 0) {
     return l10n_util::GetStringFUTF16(
          IDS_DEL_MEDIA_LICENSES_COUNTER_SITE_COMMENT,
          base::UTF8ToUTF16(media_license_result->GetOneOrigin()));
    }
    return l10n_util::GetStringUTF16(
        IDS_DEL_MEDIA_LICENSES_COUNTER_GENERAL_COMMENT);
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (pref_name == browsing_data::prefs::kDeleteHostedAppsData) {
    // Hosted apps counter.
    const HostedAppsCounter::HostedAppsResult* hosted_apps_result =
        static_cast<const HostedAppsCounter::HostedAppsResult*>(result);
    int hosted_apps_count = hosted_apps_result->Value();

    DCHECK_GE(hosted_apps_result->Value(),
              base::checked_cast<browsing_data::BrowsingDataCounter::ResultInt>(
                  hosted_apps_result->examples().size()));

    std::vector<base::string16> replacements;
    if (hosted_apps_count > 0) {
      replacements.push_back(                                     // App1,
          base::UTF8ToUTF16(hosted_apps_result->examples()[0]));
    }
    if (hosted_apps_count > 1) {
      replacements.push_back(
          base::UTF8ToUTF16(hosted_apps_result->examples()[1]));  // App2,
    }
    if (hosted_apps_count > 2) {
      replacements.push_back(l10n_util::GetPluralStringFUTF16(  // and X-2 more.
          IDS_DEL_HOSTED_APPS_COUNTER_AND_X_MORE,
          hosted_apps_count - 2));
    }

    // The output string has both the number placeholder (#) and substitution
    // placeholders ($1, $2, $3). First fetch the correct plural string first,
    // then substitute the $ placeholders.
    return base::ReplaceStringPlaceholders(
        l10n_util::GetPluralStringFUTF16(
            IDS_DEL_HOSTED_APPS_COUNTER, hosted_apps_count),
        replacements,
        nullptr);
  }
#endif

  return browsing_data::GetCounterTextFromResult(result);
}
