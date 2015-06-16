// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"

namespace data_reduction_proxy {

namespace {

// Returns the value at |index| of |list_value| as an int64.
int64 GetInt64PrefValue(const base::ListValue& list_value, size_t index) {
  int64 val = 0;
  std::string pref_value;
  bool rv = list_value.GetString(index, &pref_value);
  DCHECK(rv);
  if (rv) {
    rv = base::StringToInt64(pref_value, &val);
    DCHECK(rv);
  }
  return val;
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
  list_update->Set(index, new base::StringValue(base::Int64ToString(value)));
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

// DailyContentLengthUpdate maintains a data saving pref. The pref is a list
// of |kNumDaysInHistory| elements of daily total content lengths for the past
// |kNumDaysInHistory| days.
void RecordDailyContentLengthHistograms(
    int64 original_length,
    int64 received_length,
    int64 original_length_with_data_reduction_enabled,
    int64 received_length_with_data_reduction_enabled,
    int64 original_length_via_data_reduction_proxy,
    int64 received_length_via_data_reduction_proxy,
    int64 https_length_with_data_reduction_enabled,
    int64 short_bypass_length_with_data_reduction_enabled,
    int64 long_bypass_length_with_data_reduction_enabled,
    int64 unknown_length_with_data_reduction_enabled) {
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

  DCHECK_GE(https_length_with_data_reduction_enabled, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled_Https",
      https_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled_Https",
      (100 * https_length_with_data_reduction_enabled) / received_length);

  DCHECK_GE(short_bypass_length_with_data_reduction_enabled, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled_ShortBypass",
      short_bypass_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled_ShortBypass",
      ((100 * short_bypass_length_with_data_reduction_enabled) /
       received_length));

  DCHECK_GE(long_bypass_length_with_data_reduction_enabled, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled_LongBypass",
      long_bypass_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled_LongBypass",
      ((100 * long_bypass_length_with_data_reduction_enabled) /
       received_length));

  DCHECK_GE(unknown_length_with_data_reduction_enabled, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyContentLength_DataReductionProxyEnabled_Unknown",
      unknown_length_with_data_reduction_enabled >> 10);
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_DataReductionProxyEnabled_Unknown",
      ((100 * unknown_length_with_data_reduction_enabled) /
       received_length));

  DCHECK_GE(original_length_via_data_reduction_proxy, 0);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyOriginalContentLength_ViaDataReductionProxy",
      original_length_via_data_reduction_proxy >> 10);
  DCHECK_GE(received_length_via_data_reduction_proxy, 0);
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

class DailyContentLengthUpdate {
 public:
  DailyContentLengthUpdate(base::ListValue* update)
      : update_(update) {}

  void UpdateForDataChange(int days_since_last_update) {
    // New empty lists may have been created. Maintain the invariant that
    // there should be exactly |kNumDaysInHistory| days in the histories.
    MaintainContentLengthPrefsWindow(update_, kNumDaysInHistory);
    if (days_since_last_update) {
      MaintainContentLengthPrefForDateChange(days_since_last_update);
    }
  }

  // Update the lengths for the current day.
  void Add(int64 content_length) {
    AddInt64ToListPref(kNumDaysInHistory - 1, content_length, update_);
  }

  int64 GetListPrefValue(size_t index) {
    return ListPrefInt64Value(*update_, index);
  }

 private:
  // Update the list for date change and ensure the list has exactly |length|
  // elements. The last entry in the list will be for the current day after
  // the update.
  void MaintainContentLengthPrefForDateChange(int days_since_last_update) {
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
      update_->Clear();

      days_since_last_update = kNumDaysInHistory;
    }
    DCHECK_GE(days_since_last_update, 0);

    // Add entries for days since last update event. This will make the
    // lists longer than kNumDaysInHistory. The additional items will be cut off
    // from the head of the lists by |MaintainContentLengthPrefsWindow|, below.
    for (int i = 0;
         i < days_since_last_update && i < static_cast<int>(kNumDaysInHistory);
         ++i) {
      update_->AppendString(base::Int64ToString(0));
    }

    // Entries for new days may have been appended. Maintain the invariant that
    // there should be exactly |kNumDaysInHistory| days in the histories.
    MaintainContentLengthPrefsWindow(update_, kNumDaysInHistory);
  }

  base::ListValue* update_;
};

// DailyDataSavingUpdate maintains a pair of data saving prefs, original_update_
// and received_update_. pref_original is a list of |kNumDaysInHistory| elements
// of daily total original content lengths for the past |kNumDaysInHistory|
// days. pref_received is the corresponding list of the daily total received
// content lengths.
class DailyDataSavingUpdate {
 public:
  DailyDataSavingUpdate(base::ListValue* original,
                        base::ListValue* received)
      : original_(original),
        received_(received) {}

  void UpdateForDataChange(int days_since_last_update) {
    original_.UpdateForDataChange(days_since_last_update);
    received_.UpdateForDataChange(days_since_last_update);
  }

  // Update the lengths for the current day.
  void Add(int64 original_content_length, int64 received_content_length) {
    original_.Add(original_content_length);
    received_.Add(received_content_length);
  }

  int64 GetOriginalListPrefValue(size_t index) {
    return original_.GetListPrefValue(index);
  }
  int64 GetReceivedListPrefValue(size_t index) {
    return received_.GetListPrefValue(index);
  }

 private:
  DailyContentLengthUpdate original_;
  DailyContentLengthUpdate received_;
};

// Report UMA metrics for daily data reductions.
}  // namespace

DataReductionProxyCompressionStats::DataReductionProxyCompressionStats(
    PrefService* prefs,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const base::TimeDelta& delay)
    : pref_service_(prefs),
      task_runner_(task_runner),
      delay_(delay),
      delayed_task_posted_(false),
      pref_change_registrar_(new PrefChangeRegistrar()),
      weak_factory_(this) {
  DCHECK(prefs);
  DCHECK_GE(delay.InMilliseconds(), 0);
  Init();
}

DataReductionProxyCompressionStats::~DataReductionProxyCompressionStats() {
  DCHECK(thread_checker_.CalledOnValidThread());
  WritePrefs();
  pref_change_registrar_->RemoveAll();
  weak_factory_.InvalidateWeakPtrs();
}

void DataReductionProxyCompressionStats::Init() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delay_ == base::TimeDelta())
    return;

  // Init all int64 prefs.
  InitInt64Pref(prefs::kDailyHttpContentLengthLastUpdateDate);
  InitInt64Pref(prefs::kHttpReceivedContentLength);
  InitInt64Pref(prefs::kHttpOriginalContentLength);

  // Init all list prefs.
  InitListPref(prefs::kDailyContentLengthHttpsWithDataReductionProxyEnabled);
  InitListPref(
      prefs::kDailyContentLengthLongBypassWithDataReductionProxyEnabled);
  InitListPref(
      prefs::kDailyContentLengthShortBypassWithDataReductionProxyEnabled);
  InitListPref(prefs::kDailyContentLengthUnknownWithDataReductionProxyEnabled);
  InitListPref(prefs::kDailyContentLengthViaDataReductionProxy);
  InitListPref(prefs::kDailyContentLengthWithDataReductionProxyEnabled);
  InitListPref(prefs::kDailyHttpOriginalContentLength);
  InitListPref(prefs::kDailyHttpReceivedContentLength);
  InitListPref(prefs::kDailyOriginalContentLengthViaDataReductionProxy);
  InitListPref(prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kClearDataReductionProxyDataSavings)) {
    ClearDataSavingStatistics();
  }

  pref_change_registrar_->Init(pref_service_);
  pref_change_registrar_->Add(prefs::kUpdateDailyReceivedContentLengths,
      base::Bind(&DataReductionProxyCompressionStats::OnUpdateContentLengths,
                 weak_factory_.GetWeakPtr()));
}

void DataReductionProxyCompressionStats::OnUpdateContentLengths() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!pref_service_->GetBoolean(prefs::kUpdateDailyReceivedContentLengths))
    return;

  WritePrefs();
  pref_service_->SetBoolean(prefs::kUpdateDailyReceivedContentLengths, false);
}

void DataReductionProxyCompressionStats::UpdateContentLengths(
    int64 received_content_length,
    int64 original_content_length,
    bool data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int64 total_received = GetInt64(
      data_reduction_proxy::prefs::kHttpReceivedContentLength);
  int64 total_original = GetInt64(
      data_reduction_proxy::prefs::kHttpOriginalContentLength);
  total_received += received_content_length;
  total_original += original_content_length;
  SetInt64(data_reduction_proxy::prefs::kHttpReceivedContentLength,
           total_received);
  SetInt64(data_reduction_proxy::prefs::kHttpOriginalContentLength,
           total_original);

  RecordContentLengthPrefs(
      received_content_length,
      original_content_length,
      data_reduction_proxy_enabled,
      request_type,
      base::Time::Now());
}

void DataReductionProxyCompressionStats::InitInt64Pref(const char* pref) {
  int64 pref_value = pref_service_->GetInt64(pref);
  pref_map_[pref] = pref_value;
}

void DataReductionProxyCompressionStats::InitListPref(const char* pref) {
  scoped_ptr<base::ListValue> pref_value = scoped_ptr<base::ListValue>(
      pref_service_->GetList(pref)->DeepCopy());
  list_pref_map_.add(pref, pref_value.Pass());
}

int64 DataReductionProxyCompressionStats::GetInt64(const char* pref_path) {
  if (delay_ == base::TimeDelta())
    return pref_service_->GetInt64(pref_path);

  DataReductionProxyPrefMap::iterator iter = pref_map_.find(pref_path);
  return iter->second;
}

void DataReductionProxyCompressionStats::SetInt64(const char* pref_path,
                                                  int64 pref_value) {
  if (delay_ == base::TimeDelta()) {
    pref_service_->SetInt64(pref_path, pref_value);
    return;
  }

  DelayedWritePrefs();
  pref_map_[pref_path] = pref_value;
}

base::ListValue* DataReductionProxyCompressionStats::GetList(
    const char* pref_path) {
  if (delay_ == base::TimeDelta())
    return ListPrefUpdate(pref_service_, pref_path).Get();

  DelayedWritePrefs();
  return list_pref_map_.get(pref_path);
}

void DataReductionProxyCompressionStats::WritePrefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delay_ == base::TimeDelta())
    return;

  for (DataReductionProxyPrefMap::iterator iter = pref_map_.begin();
       iter != pref_map_.end(); ++iter) {
    pref_service_->SetInt64(iter->first, iter->second);
  }

  for (DataReductionProxyListPrefMap::iterator iter = list_pref_map_.begin();
       iter != list_pref_map_.end(); ++iter) {
    TransferList(*(iter->second),
                 ListPrefUpdate(pref_service_, iter->first).Get());
  }

  delayed_task_posted_ = false;
}

base::Value*
DataReductionProxyCompressionStats::HistoricNetworkStatsInfoToValue() {
  DCHECK(thread_checker_.CalledOnValidThread());
  int64 total_received = GetInt64(prefs::kHttpReceivedContentLength);
  int64 total_original = GetInt64(prefs::kHttpOriginalContentLength);

  base::DictionaryValue* dict = new base::DictionaryValue();
  // Use strings to avoid overflow. base::Value only supports 32-bit integers.
  dict->SetString("historic_received_content_length",
                  base::Int64ToString(total_received));
  dict->SetString("historic_original_content_length",
                  base::Int64ToString(total_original));
  return dict;
}

int64 DataReductionProxyCompressionStats::GetLastUpdateTime() {
  int64 last_update_internal = GetInt64(
      prefs::kDailyHttpContentLengthLastUpdateDate);
  base::Time last_update = base::Time::FromInternalValue(last_update_internal);
  return static_cast<int64>(last_update.ToJsTime());
}

void DataReductionProxyCompressionStats::ResetStatistics() {
  base::ListValue* original_update =
      GetList(prefs::kDailyHttpOriginalContentLength);
  base::ListValue* received_update =
      GetList(prefs::kDailyHttpReceivedContentLength);
  original_update->Clear();
  received_update->Clear();
  for (size_t i = 0; i < kNumDaysInHistory; ++i) {
    original_update->AppendString(base::Int64ToString(0));
    received_update->AppendString(base::Int64ToString(0));
  }
}

ContentLengthList DataReductionProxyCompressionStats::GetDailyContentLengths(
    const char* pref_name) {
  ContentLengthList content_lengths;
  const base::ListValue* list_value = GetList(pref_name);
  if (list_value->GetSize() == kNumDaysInHistory) {
    for (size_t i = 0; i < kNumDaysInHistory; ++i)
      content_lengths.push_back(GetInt64PrefValue(*list_value, i));
  }
  return content_lengths;
}

void DataReductionProxyCompressionStats::GetContentLengths(
    unsigned int days,
    int64* original_content_length,
    int64* received_content_length,
    int64* last_update_time) {
  DCHECK_LE(days, kNumDaysInHistory);

  const base::ListValue* original_list =
      GetList(prefs::kDailyHttpOriginalContentLength);
  const base::ListValue* received_list =
      GetList(prefs::kDailyHttpReceivedContentLength);

  if (original_list->GetSize() != kNumDaysInHistory ||
      received_list->GetSize() != kNumDaysInHistory) {
    *original_content_length = 0L;
    *received_content_length = 0L;
    *last_update_time = 0L;
    return;
  }

  int64 orig = 0L;
  int64 recv = 0L;
  // Include days from the end of the list going backwards.
  for (size_t i = kNumDaysInHistory - days;
       i < kNumDaysInHistory; ++i) {
    orig += GetInt64PrefValue(*original_list, i);
    recv += GetInt64PrefValue(*received_list, i);
  }
  *original_content_length = orig;
  *received_content_length = recv;
  *last_update_time = GetInt64(prefs::kDailyHttpContentLengthLastUpdateDate);
}

void DataReductionProxyCompressionStats::ClearDataSavingStatistics() {
  list_pref_map_.get(
      prefs::kDailyContentLengthHttpsWithDataReductionProxyEnabled)->Clear();
  list_pref_map_
      .get(prefs::kDailyContentLengthLongBypassWithDataReductionProxyEnabled)
      ->Clear();
  list_pref_map_
      .get(prefs::kDailyContentLengthShortBypassWithDataReductionProxyEnabled)
      ->Clear();
  list_pref_map_
      .get(prefs::kDailyContentLengthUnknownWithDataReductionProxyEnabled)
      ->Clear();
  list_pref_map_.get(prefs::kDailyContentLengthViaDataReductionProxy)->Clear();
  list_pref_map_.get(prefs::kDailyContentLengthWithDataReductionProxyEnabled)
      ->Clear();
  list_pref_map_.get(prefs::kDailyHttpOriginalContentLength)->Clear();
  list_pref_map_.get(prefs::kDailyHttpReceivedContentLength)->Clear();
  list_pref_map_.get(prefs::kDailyOriginalContentLengthViaDataReductionProxy)
      ->Clear();
  list_pref_map_
      .get(prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled)
      ->Clear();

  WritePrefs();
}

void DataReductionProxyCompressionStats::DelayedWritePrefs() {
  // Only write after the first time posting the task.
  if (delayed_task_posted_)
    return;

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyCompressionStats::WritePrefs,
                 weak_factory_.GetWeakPtr()),
      delay_);

  delayed_task_posted_ = true;
}

void DataReductionProxyCompressionStats::TransferList(
    const base::ListValue& from_list,
    base::ListValue* to_list) {
  to_list->Clear();
  for (size_t i = 0; i < from_list.GetSize(); ++i) {
    to_list->Set(i, new base::StringValue(base::Int64ToString(
        GetListPrefInt64Value(from_list, i))));
  }
}

int64 DataReductionProxyCompressionStats::GetListPrefInt64Value(
    const base::ListValue& list,
    size_t index) {
  std::string string_value;
  if (!list.GetString(index, &string_value)) {
    NOTREACHED();
    return 0;
  }

  int64 value = 0;
  bool rv = base::StringToInt64(string_value, &value);
  DCHECK(rv);
  return value;
}

void DataReductionProxyCompressionStats::RecordContentLengthPrefs(
    int64 received_content_length,
    int64 original_content_length,
    bool with_data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type,
    base::Time now) {
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
  int64 then_internal = GetInt64(
      data_reduction_proxy::prefs::kDailyHttpContentLengthLastUpdateDate);

  // Local midnight could have been shifted due to time zone change.
  // If time is null then don't care if midnight will be wrong shifted due to
  // time zone change because it's still too much time ago.
  base::Time then_midnight = base::Time::FromInternalValue(then_internal);
  if (!then_midnight.is_null()) {
    then_midnight = then_midnight.LocalMidnight();
  }
  base::Time midnight = now.LocalMidnight();

  int days_since_last_update = (midnight - then_midnight).InDays();

  // Each day, we calculate the total number of bytes received and the total
  // size of all corresponding resources before any data-reducing recompression
  // is applied. These values are used to compute the data savings realized
  // by applying our compression techniques. Totals for the last
  // |kNumDaysInHistory| days are maintained.
  DailyDataSavingUpdate total(
      GetList(data_reduction_proxy::prefs::kDailyHttpOriginalContentLength),
      GetList(data_reduction_proxy::prefs::kDailyHttpReceivedContentLength));
  total.UpdateForDataChange(days_since_last_update);

  DailyDataSavingUpdate proxy_enabled(
      GetList(data_reduction_proxy::prefs::
          kDailyOriginalContentLengthWithDataReductionProxyEnabled),
      GetList(data_reduction_proxy::prefs::
          kDailyContentLengthWithDataReductionProxyEnabled));
  proxy_enabled.UpdateForDataChange(days_since_last_update);

  DailyDataSavingUpdate via_proxy(
      GetList(data_reduction_proxy::prefs::
          kDailyOriginalContentLengthViaDataReductionProxy),
      GetList(data_reduction_proxy::prefs::
          kDailyContentLengthViaDataReductionProxy));
  via_proxy.UpdateForDataChange(days_since_last_update);

  DailyContentLengthUpdate https(
      GetList(data_reduction_proxy::prefs::
          kDailyContentLengthHttpsWithDataReductionProxyEnabled));
  https.UpdateForDataChange(days_since_last_update);

  DailyContentLengthUpdate short_bypass(
      GetList(data_reduction_proxy::prefs::
          kDailyContentLengthShortBypassWithDataReductionProxyEnabled));
  short_bypass.UpdateForDataChange(days_since_last_update);

  DailyContentLengthUpdate long_bypass(
      GetList(data_reduction_proxy::prefs::
          kDailyContentLengthLongBypassWithDataReductionProxyEnabled));
  long_bypass.UpdateForDataChange(days_since_last_update);

  DailyContentLengthUpdate unknown(
      GetList(data_reduction_proxy::prefs::
          kDailyContentLengthUnknownWithDataReductionProxyEnabled));
  unknown.UpdateForDataChange(days_since_last_update);

  total.Add(original_content_length, received_content_length);
  if (with_data_reduction_proxy_enabled) {
    proxy_enabled.Add(original_content_length, received_content_length);
    // Ignore data source cases, if exist, when
    // "with_data_reduction_proxy_enabled == false"
    switch (request_type) {
      case VIA_DATA_REDUCTION_PROXY:
        via_proxy.Add(original_content_length, received_content_length);
        break;
      case HTTPS:
        https.Add(received_content_length);
        break;
      case SHORT_BYPASS:
        short_bypass.Add(received_content_length);
        break;
      case LONG_BYPASS:
        long_bypass.Add(received_content_length);
        break;
      case UNKNOWN_TYPE:
        unknown.Add(received_content_length);
        break;
    }
  }

  if (days_since_last_update) {
    // Record the last update time in microseconds in UTC.
    SetInt64(
        data_reduction_proxy::prefs::kDailyHttpContentLengthLastUpdateDate,
        midnight.ToInternalValue());

    // A new day. Report the previous day's data if exists. We'll lose usage
    // data if the last time Chrome was run was more than a day ago.
    // Here, we prefer collecting less data but the collected data is
    // associated with an accurate date.
    if (days_since_last_update == 1) {
      RecordUserVisibleDataSavings();
      // The previous day's data point is the second one from the tail.
      // Therefore (kNumDaysInHistory - 2) below.
      RecordDailyContentLengthHistograms(
          total.GetOriginalListPrefValue(kNumDaysInHistory - 2),
          total.GetReceivedListPrefValue(kNumDaysInHistory - 2),
          proxy_enabled.GetOriginalListPrefValue(kNumDaysInHistory - 2),
          proxy_enabled.GetReceivedListPrefValue(kNumDaysInHistory - 2),
          via_proxy.GetOriginalListPrefValue(kNumDaysInHistory - 2),
          via_proxy.GetReceivedListPrefValue(kNumDaysInHistory - 2),
          https.GetListPrefValue(kNumDaysInHistory - 2),
          short_bypass.GetListPrefValue(kNumDaysInHistory - 2),
          long_bypass.GetListPrefValue(kNumDaysInHistory - 2),
          unknown.GetListPrefValue(kNumDaysInHistory - 2));
    }
  }
}

void DataReductionProxyCompressionStats::RecordUserVisibleDataSavings() {
  int64 original_content_length;
  int64 received_content_length;
  int64 last_update_internal;
  GetContentLengths(kNumDaysInHistorySummary, &original_content_length,
                    &received_content_length, &last_update_internal);

  if (original_content_length == 0)
    return;

  int64 user_visible_savings_bytes =
      original_content_length - received_content_length;
  int user_visible_savings_percent =
      user_visible_savings_bytes * 100 / original_content_length;
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyUserVisibleSavingsPercent_DataReductionProxyEnabled",
      user_visible_savings_percent);
  UMA_HISTOGRAM_COUNTS(
      "Net.DailyUserVisibleSavingsSize_DataReductionProxyEnabled",
      user_visible_savings_bytes >> 10);
}

}  // namespace data_reduction_proxy
