// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_data_saving_metrics.h"

#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/pref_names.h"

#if defined(OS_ANDROID) || defined(OS_IOS)
namespace {

// The number of days of history stored in the content lengths prefs.
const size_t kNumDaysInHistory = 60;

// Increments an int64, stored as a string, in a ListPref at the specified
// index.  The value must already exist and be a string representation of a
// number.
void AddInt64ToListPref(size_t index,
                        int64 length,
                        base::ListValue* list_update) {
  int64 value = 0;
  std::string old_string_value;
  bool rv = list_update->GetString(index, &old_string_value);
  DCHECK(rv);
  if (rv) {
    rv = base::StringToInt64(old_string_value, &value);
    DCHECK(rv);
  }
  value += length;
  list_update->Set(index, Value::CreateStringValue(base::Int64ToString(value)));
}

int64 ListPrefInt64Value(const base::ListValue& list_update, size_t index) {
  std::string string_value;
  if (!list_update.GetString(index, &string_value)) {
    NOTREACHED();
    return 0;
  }

  int64 value = 0;
  bool rv = base::StringToInt64(string_value, &value);
  DCHECK(rv);
  return value;
}

// Report UMA metrics for daily data reductions.
void RecordDailyContentLengthHistograms(
    int64 original_length,
    int64 received_length,
    int64 original_length_with_data_reduction_enabled,
    int64 received_length_with_data_reduction_enabled,
    int64 original_length_via_data_reduction_proxy,
    int64 received_length_via_data_reduction_proxy) {
  // Report daily UMA only for days having received content.
  if (original_length <= 0 || received_length <= 0)
    return;

  // Record metrics in KB.
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyOriginalContentLength", original_length >> 10);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength", received_length >> 10);
  int percent = 0;
  // UMA percentage cannot be negative.
  if (original_length > received_length) {
    percent = (100 * (original_length - received_length)) / original_length;
  }
  UMA_HISTOGRAM_PERCENTAGE("Net.DailyContentSavingPercent", percent);

  if (original_length_with_data_reduction_enabled <= 0 ||
      received_length_with_data_reduction_enabled <= 0) {
    return;
  }

  UMA_HISTOGRAM_COUNTS(
      "Net.DailyOriginalContentLength_DataReductionProxyEnabled",
      original_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled",
      received_length_with_data_reduction_enabled >> 10);

  int percent_data_reduction_proxy_enabled = 0;
  // UMA percentage cannot be negative.
  if (original_length_with_data_reduction_enabled >
      received_length_with_data_reduction_enabled) {
    percent_data_reduction_proxy_enabled =
        100 * (original_length_with_data_reduction_enabled -
               received_length_with_data_reduction_enabled) /
        original_length_with_data_reduction_enabled;
  }
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentSavingPercent_DataReductionProxyEnabled",
      percent_data_reduction_proxy_enabled);

  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled",
      (100 * received_length_with_data_reduction_enabled) / received_length);

  if (original_length_via_data_reduction_proxy <= 0 ||
      received_length_via_data_reduction_proxy <= 0) {
    return;
  }

  UMA_HISTOGRAM_COUNTS(
      "Net.DailyOriginalContentLength_ViaDataReductionProxy",
      original_length_via_data_reduction_proxy >> 10);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_ViaDataReductionProxy",
      received_length_via_data_reduction_proxy >> 10);
  int percent_via_data_reduction_proxy = 0;
  if (original_length_via_data_reduction_proxy >
      received_length_via_data_reduction_proxy) {
    percent_via_data_reduction_proxy =
        100 * (original_length_via_data_reduction_proxy -
               received_length_via_data_reduction_proxy) /
        original_length_via_data_reduction_proxy;
  }
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentSavingPercent_ViaDataReductionProxy",
      percent_via_data_reduction_proxy);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_ViaDataReductionProxy",
      (100 * received_length_via_data_reduction_proxy) / received_length);
}

// Ensure list has exactly |length| elements, either by truncating at the
// front, or appending "0"'s to the back.
void MaintainContentLengthPrefsWindow(base::ListValue* list, size_t length) {
  // Remove data for old days from the front.
  while (list->GetSize() > length)
    list->Remove(0, NULL);
  // Newly added lists are empty. Add entries to back to fill the window,
  // each initialized to zero.
  while (list->GetSize() < length)
    list->AppendString(base::Int64ToString(0));
  DCHECK_EQ(length, list->GetSize());
}

// Update list for date change and ensure list has exactly |length| elements.
// The last entry in each list will be for the current day after the update.
void MaintainContentLengthPrefsForDateChange(
    base::ListValue* original_update,
    base::ListValue* received_update,
    int days_since_last_update) {
  if (days_since_last_update == -1) {
    // The system may go backwards in time by up to a day for legitimate
    // reasons, such as with changes to the time zone. In such cases, we
    // keep adding to the current day.
    // Note: we accept the fact that some reported data is shifted to
    // the adjacent day if users travel back and forth across time zones.
    days_since_last_update = 0;
  } else if (days_since_last_update < -1) {
    // Erase all entries if the system went backwards in time by more than
    // a day.
    original_update->Clear();
    received_update->Clear();

    days_since_last_update = kNumDaysInHistory;
  }
  DCHECK_GE(days_since_last_update, 0);

  // Add entries for days since last update event. This will make the
  // lists longer than kNumDaysInHistory. The additional items will be cut off
  // from the head of the lists by |MaintainContentLengthPrefsWindow|, below.
  for (int i = 0;
       i < days_since_last_update && i < static_cast<int>(kNumDaysInHistory);
       ++i) {
    original_update->AppendString(base::Int64ToString(0));
    received_update->AppendString(base::Int64ToString(0));
  }

  // Entries for new days may have been appended. Maintain the invariant that
  // there should be exactly |kNumDaysInHistory| days in the histories.
  MaintainContentLengthPrefsWindow(original_update, kNumDaysInHistory);
  MaintainContentLengthPrefsWindow(received_update, kNumDaysInHistory);
}

// DailyDataSavingUpdate maintains a pair of data saving prefs, original_update_
// and received_update_. pref_original is a list of |kNumDaysInHistory| elements
// of daily total original content lengths for the past |kNumDaysInHistory|
// days. pref_received is the corresponding list of the daily total received
// content lengths.
class DailyDataSavingUpdate {
 public:
  DailyDataSavingUpdate(
      const char* pref_original, const char* pref_received,
      PrefService* pref_service)
      : pref_original_(pref_original),
        pref_received_(pref_received),
        original_update_(pref_service, pref_original_),
        received_update_(pref_service, pref_received_) {
  }

  void UpdateForDataChange(int days_since_last_update) {
    // New empty lists may have been created. Maintain the invariant that
    // there should be exactly |kNumDaysInHistory| days in the histories.
    MaintainContentLengthPrefsWindow(original_update_.Get(), kNumDaysInHistory);
    MaintainContentLengthPrefsWindow(received_update_.Get(), kNumDaysInHistory);
    if (days_since_last_update) {
      MaintainContentLengthPrefsForDateChange(
          original_update_.Get(), received_update_.Get(),
          days_since_last_update);
    }
  }

  // Update the lengths for the current day.
  void Add(int original_content_length, int received_content_length) {
    AddInt64ToListPref(
        kNumDaysInHistory - 1, original_content_length, original_update_.Get());
    AddInt64ToListPref(
        kNumDaysInHistory - 1, received_content_length, received_update_.Get());
  }

  int64 GetOriginalListPrefValue(size_t index) {
    return ListPrefInt64Value(*original_update_, index);
  }
  int64 GetReceivedListPrefValue(size_t index) {
    return ListPrefInt64Value(*received_update_, index);
  }

 private:
  const char* pref_original_;
  const char* pref_received_;
  ListPrefUpdate original_update_;
  ListPrefUpdate received_update_;
};

}  // namespace
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

namespace chrome_browser_net {

#if defined(OS_ANDROID) || defined(OS_IOS)
void UpdateContentLengthPrefsForDataReductionProxy(
    int received_content_length, int original_content_length,
    bool with_data_reduction_proxy_enabled, bool via_data_reduction_proxy,
    base::Time now, PrefService* prefs) {
  // TODO(bengr): Remove this check once the underlying cause of
  // http://crbug.com/287821 is fixed. For now, only continue if the current
  // year is reported as being between 1972 and 2970.
  base::TimeDelta time_since_unix_epoch = now - base::Time::UnixEpoch();
  const int kMinDaysSinceUnixEpoch = 365 * 2;  // 2 years.
  const int kMaxDaysSinceUnixEpoch = 365 * 1000;  // 1000 years.
  if (time_since_unix_epoch.InDays() < kMinDaysSinceUnixEpoch ||
      time_since_unix_epoch.InDays() > kMaxDaysSinceUnixEpoch) {
    return;
  }

  // Determine how many days it has been since the last update.
  int64 then_internal = prefs->GetInt64(
      prefs::kDailyHttpContentLengthLastUpdateDate);
  // Local midnight could have been shifted due to time zone change.
  base::Time then_midnight =
      base::Time::FromInternalValue(then_internal).LocalMidnight();
  base::Time midnight = now.LocalMidnight();
  int days_since_last_update = (midnight - then_midnight).InDays();

  // Each day, we calculate the total number of bytes received and the total
  // size of all corresponding resources before any data-reducing recompression
  // is applied. These values are used to compute the data savings realized
  // by applying our compression techniques. Totals for the last
  // |kNumDaysInHistory| days are maintained.
  DailyDataSavingUpdate total(
      prefs::kDailyHttpOriginalContentLength,
      prefs::kDailyHttpReceivedContentLength,
      prefs);
  total.UpdateForDataChange(days_since_last_update);

  DailyDataSavingUpdate proxy_enabled(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled,
      prefs::kDailyContentLengthWithDataReductionProxyEnabled,
      prefs);
  proxy_enabled.UpdateForDataChange(days_since_last_update);

  DailyDataSavingUpdate via_proxy(
      prefs::kDailyOriginalContentLengthViaDataReductionProxy,
      prefs::kDailyContentLengthViaDataReductionProxy,
      prefs);
  via_proxy.UpdateForDataChange(days_since_last_update);

  total.Add(original_content_length, received_content_length);
  if (with_data_reduction_proxy_enabled) {
    proxy_enabled.Add(original_content_length, received_content_length);
    // Ignore cases, if exist, when
    // "with_data_reduction_proxy_enabled == false", and
    // "via_data_reduction_proxy == true"
    if (via_data_reduction_proxy) {
      via_proxy.Add(original_content_length, received_content_length);
    }
  }

  if (days_since_last_update) {
    // Record the last update time in microseconds in UTC.
    prefs->SetInt64(prefs::kDailyHttpContentLengthLastUpdateDate,
                    midnight.ToInternalValue());

    // A new day. Report the previous day's data if exists. We'll lose usage
    // data if the last time Chrome was run was more than a day ago.
    // Here, we prefer collecting less data but the collected data is
    // associated with an accurate date.
    if (days_since_last_update == 1) {
      // The previous day's data point is the second one from the tail.
      RecordDailyContentLengthHistograms(
          total.GetOriginalListPrefValue(kNumDaysInHistory - 2),
          total.GetReceivedListPrefValue(kNumDaysInHistory - 2),
          proxy_enabled.GetOriginalListPrefValue(kNumDaysInHistory - 2),
          proxy_enabled.GetReceivedListPrefValue(kNumDaysInHistory - 2),
          via_proxy.GetOriginalListPrefValue(kNumDaysInHistory - 2),
          via_proxy.GetReceivedListPrefValue(kNumDaysInHistory - 2));
    }
  }
}
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

void UpdateContentLengthPrefs(
    int received_content_length, int original_content_length,
    bool with_data_reduction_proxy_enabled, bool via_data_reduction_proxy,
    PrefService* prefs) {
  int64 total_received = prefs->GetInt64(prefs::kHttpReceivedContentLength);
  int64 total_original = prefs->GetInt64(prefs::kHttpOriginalContentLength);
  total_received += received_content_length;
  total_original += original_content_length;
  prefs->SetInt64(prefs::kHttpReceivedContentLength, total_received);
  prefs->SetInt64(prefs::kHttpOriginalContentLength, total_original);

#if defined(OS_ANDROID) || defined(OS_IOS)
  UpdateContentLengthPrefsForDataReductionProxy(
      received_content_length,
      original_content_length,
      with_data_reduction_proxy_enabled,
      via_data_reduction_proxy,
      base::Time::Now(),
      prefs);
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

}

}  // namespace chrome_browser_net
