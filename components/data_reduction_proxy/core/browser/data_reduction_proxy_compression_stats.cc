// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_usage_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "net/base/mime_util.h"

namespace data_reduction_proxy {

namespace {

#define CONCAT(a, b) a##b
// CONCAT1 provides extra level of indirection so that __LINE__ macro expands.
#define CONCAT1(a, b) CONCAT(a, b)
#define UNIQUE_VARNAME CONCAT1(var_, __LINE__)
// We need to use a macro instead of a method because UMA_HISTOGRAM_COUNTS
// requires its first argument to be an inline string and not a variable.
#define RECORD_INT64PREF_TO_HISTOGRAM(pref, uma)     \
  int64_t UNIQUE_VARNAME = GetInt64(pref);           \
  if (UNIQUE_VARNAME > 0) {                          \
    UMA_HISTOGRAM_COUNTS(uma, UNIQUE_VARNAME >> 10); \
  }

// Returns the value at |index| of |list_value| as an int64_t.
int64_t GetInt64PrefValue(const base::ListValue& list_value, size_t index) {
  int64_t val = 0;
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
    list->Remove(0, nullptr);
  // Newly added lists are empty. Add entries to back to fill the window,
  // each initialized to zero.
  while (list->GetSize() < length)
    list->AppendString(base::Int64ToString(0));
  DCHECK_EQ(length, list->GetSize());
}

// Increments an int64_t, stored as a string, in a ListPref at the specified
// index.  The value must already exist and be a string representation of a
// number.
void AddInt64ToListPref(size_t index,
                        int64_t length,
                        base::ListValue* list_update) {
  int64_t value = GetInt64PrefValue(*list_update, index) + length;
  list_update->Set(index,
                   std::make_unique<base::Value>(base::Int64ToString(value)));
}

// DailyContentLengthUpdate maintains a data saving pref. The pref is a list
// of |kNumDaysInHistory| elements of daily total content lengths for the past
// |kNumDaysInHistory| days.
void RecordDailyContentLengthHistograms(
    int64_t original_length,
    int64_t received_length,
    int64_t original_length_with_data_reduction_enabled,
    int64_t received_length_with_data_reduction_enabled,
    int64_t original_length_via_data_reduction_proxy,
    int64_t received_length_via_data_reduction_proxy,
    int64_t https_length_with_data_reduction_enabled,
    int64_t short_bypass_length_with_data_reduction_enabled,
    int64_t long_bypass_length_with_data_reduction_enabled,
    int64_t unknown_length_with_data_reduction_enabled) {
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
      "Net.DailyContentLength_DataReductionProxyEnabled_UnknownBypass",
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
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentPercent_ViaDataReductionProxy",
      (100 * received_length_via_data_reduction_proxy) / received_length);

  if (original_length_via_data_reduction_proxy <= 0)
    return;
  int percent_savings_via_data_reduction_proxy = 0;
  if (original_length_via_data_reduction_proxy >
      received_length_via_data_reduction_proxy) {
    percent_savings_via_data_reduction_proxy =
        100 * (original_length_via_data_reduction_proxy -
               received_length_via_data_reduction_proxy) /
        original_length_via_data_reduction_proxy;
  }
  UMA_HISTOGRAM_PERCENTAGE(
      "Net.DailyContentSavingPercent_ViaDataReductionProxy",
      percent_savings_via_data_reduction_proxy);
}

void RecordSavingsClearedNegativeClockMetric(int days_since_last_update) {
  // Data savings are cleared if the system clock moved back by more than
  // one day.
  UMA_HISTOGRAM_BOOLEAN("DataReductionProxy.SavingsCleared.NegativeSystemClock",
                        days_since_last_update < -1);
}

}  // namespace

class DataReductionProxyCompressionStats::DailyContentLengthUpdate {
 public:
  DailyContentLengthUpdate(
      DataReductionProxyCompressionStats* compression_stats,
      const char* pref_path)
      : update_(nullptr),
        compression_stats_(compression_stats),
        pref_path_(pref_path) {}

  void UpdateForDateChange(int days_since_last_update) {
    if (days_since_last_update) {
      MaybeInitialize();
      MaintainContentLengthPrefForDateChange(days_since_last_update);
    }
  }

  // Update the lengths for the current day.
  void Add(int64_t content_length) {
    if (content_length != 0) {
      MaybeInitialize();
      AddInt64ToListPref(kNumDaysInHistory - 1, content_length, update_);
    }
  }

  int64_t GetListPrefValue(size_t index) {
    MaybeInitialize();
    return GetInt64PrefValue(*update_, index);
  }

 private:
  void MaybeInitialize() {
    if (update_)
      return;

    update_ = compression_stats_->GetList(pref_path_);
    // New empty lists may have been created. Maintain the invariant that
    // there should be exactly |kNumDaysInHistory| days in the histories.
    MaintainContentLengthPrefsWindow(update_, kNumDaysInHistory);
  }

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

  // Non-owned. Lazily initialized, set to nullptr until initialized.
  base::ListValue* update_;
  // Non-owned pointer.
  DataReductionProxyCompressionStats* compression_stats_;
  // The path of the content length pref for |this|.
  const char* pref_path_;

  DISALLOW_COPY_AND_ASSIGN(DailyContentLengthUpdate);
};

// DailyDataSavingUpdate maintains a pair of data saving prefs, original_update_
// and received_update_. pref_original is a list of |kNumDaysInHistory| elements
// of daily total original content lengths for the past |kNumDaysInHistory|
// days. pref_received is the corresponding list of the daily total received
// content lengths.
class DataReductionProxyCompressionStats::DailyDataSavingUpdate {
 public:
  DailyDataSavingUpdate(DataReductionProxyCompressionStats* compression_stats,
                        const char* original_pref_path,
                        const char* received_pref_path)
      : original_(compression_stats, original_pref_path),
        received_(compression_stats, received_pref_path) {}

  void UpdateForDateChange(int days_since_last_update) {
    original_.UpdateForDateChange(days_since_last_update);
    received_.UpdateForDateChange(days_since_last_update);
  }

  // Update the lengths for the current day.
  void Add(int64_t original_content_length, int64_t received_content_length) {
    original_.Add(original_content_length);
    received_.Add(received_content_length);
  }

  int64_t GetOriginalListPrefValue(size_t index) {
    return original_.GetListPrefValue(index);
  }
  int64_t GetReceivedListPrefValue(size_t index) {
    return received_.GetListPrefValue(index);
  }

 private:
  DailyContentLengthUpdate original_;
  DailyContentLengthUpdate received_;

  DISALLOW_COPY_AND_ASSIGN(DailyDataSavingUpdate);
};

DataReductionProxyCompressionStats::DataReductionProxyCompressionStats(
    DataReductionProxyService* service,
    PrefService* prefs,
    const base::TimeDelta& delay)
    : service_(service),
      pref_service_(prefs),
      delay_(delay),
      data_usage_map_is_dirty_(false),
      session_total_received_(0),
      session_total_original_(0),
      current_data_usage_load_status_(NOT_LOADED),
      weak_factory_(this) {
  DCHECK(service);
  DCHECK(prefs);
  DCHECK_GE(delay.InMilliseconds(), 0);
  Init();
}

DataReductionProxyCompressionStats::~DataReductionProxyCompressionStats() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (current_data_usage_load_status_ == LOADED)
    PersistDataUsage();

  WritePrefs();
}

void DataReductionProxyCompressionStats::Init() {
  DCHECK(thread_checker_.CalledOnValidThread());

  data_usage_reporting_enabled_.Init(
      prefs::kDataUsageReportingEnabled, pref_service_,
      base::Bind(
          &DataReductionProxyCompressionStats::OnDataUsageReportingPrefChanged,
          weak_factory_.GetWeakPtr()));

  if (data_usage_reporting_enabled_.GetValue()) {
    current_data_usage_load_status_ = LOADING;
    service_->LoadCurrentDataUsageBucket(base::Bind(
        &DataReductionProxyCompressionStats::OnCurrentDataUsageLoaded,
        weak_factory_.GetWeakPtr()));
  }

  if (delay_.is_zero())
    return;

  // Init all int64_t prefs.
  InitInt64Pref(prefs::kDailyHttpContentLengthLastUpdateDate);
  InitInt64Pref(prefs::kHttpReceivedContentLength);
  InitInt64Pref(prefs::kHttpOriginalContentLength);

  InitInt64Pref(prefs::kDailyHttpOriginalContentLengthApplication);
  InitInt64Pref(prefs::kDailyHttpOriginalContentLengthVideo);
  InitInt64Pref(prefs::kDailyHttpOriginalContentLengthUnknown);
  InitInt64Pref(prefs::kDailyHttpReceivedContentLengthApplication);
  InitInt64Pref(prefs::kDailyHttpReceivedContentLengthVideo);
  InitInt64Pref(prefs::kDailyHttpReceivedContentLengthUnknown);

  InitInt64Pref(
      prefs::kDailyOriginalContentLengthViaDataReductionProxyApplication);
  InitInt64Pref(prefs::kDailyOriginalContentLengthViaDataReductionProxyVideo);
  InitInt64Pref(prefs::kDailyOriginalContentLengthViaDataReductionProxyUnknown);
  InitInt64Pref(prefs::kDailyContentLengthViaDataReductionProxyApplication);
  InitInt64Pref(prefs::kDailyContentLengthViaDataReductionProxyVideo);
  InitInt64Pref(prefs::kDailyContentLengthViaDataReductionProxyUnknown);

  InitInt64Pref(
      prefs::
          kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication);
  InitInt64Pref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo);
  InitInt64Pref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown);
  InitInt64Pref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabledApplication);
  InitInt64Pref(prefs::kDailyContentLengthWithDataReductionProxyEnabledVideo);
  InitInt64Pref(prefs::kDailyContentLengthWithDataReductionProxyEnabledUnknown);

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
}

void DataReductionProxyCompressionStats::RecordDataUseWithMimeType(
    int64_t data_used,
    int64_t original_size,
    bool data_saver_enabled,
    DataReductionProxyRequestType request_type,
    const std::string& mime_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("loader",
               "DataReductionProxyCompressionStats::RecordDataUseWithMimeType")
  session_total_received_ += data_used;
  session_total_original_ += original_size;

  IncreaseInt64Pref(data_reduction_proxy::prefs::kHttpReceivedContentLength,
                    data_used);
  IncreaseInt64Pref(data_reduction_proxy::prefs::kHttpOriginalContentLength,
                    original_size);

  RecordRequestSizePrefs(data_used, original_size, data_saver_enabled,
                         request_type, mime_type, base::Time::Now());
}

void DataReductionProxyCompressionStats::InitInt64Pref(const char* pref) {
  int64_t pref_value = pref_service_->GetInt64(pref);
  pref_map_[pref] = pref_value;
}

void DataReductionProxyCompressionStats::InitListPref(const char* pref) {
  std::unique_ptr<base::ListValue> pref_value =
      std::unique_ptr<base::ListValue>(
          pref_service_->GetList(pref)->DeepCopy());
  list_pref_map_[pref] = std::move(pref_value);
}

int64_t DataReductionProxyCompressionStats::GetInt64(const char* pref_path) {
  if (delay_.is_zero())
    return pref_service_->GetInt64(pref_path);

  DataReductionProxyPrefMap::iterator iter = pref_map_.find(pref_path);
  return iter->second;
}

void DataReductionProxyCompressionStats::SetInt64(const char* pref_path,
                                                  int64_t pref_value) {
  if (delay_.is_zero()) {
    pref_service_->SetInt64(pref_path, pref_value);
    return;
  }

  DelayedWritePrefs();
  pref_map_[pref_path] = pref_value;
}

void DataReductionProxyCompressionStats::IncreaseInt64Pref(
    const char* pref_path,
    int64_t delta) {
  SetInt64(pref_path, GetInt64(pref_path) + delta);
}

base::ListValue* DataReductionProxyCompressionStats::GetList(
    const char* pref_path) {
  if (delay_.is_zero())
    return ListPrefUpdate(pref_service_, pref_path).Get();

  DelayedWritePrefs();
  auto it = list_pref_map_.find(pref_path);
  if (it == list_pref_map_.end())
    return nullptr;
  return it->second.get();
}

void DataReductionProxyCompressionStats::WritePrefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delay_.is_zero())
    return;

  for (DataReductionProxyPrefMap::iterator iter = pref_map_.begin();
       iter != pref_map_.end(); ++iter) {
    pref_service_->SetInt64(iter->first, iter->second);
  }

  for (auto iter = list_pref_map_.begin(); iter != list_pref_map_.end();
       ++iter) {
    TransferList(*(iter->second.get()),
                 ListPrefUpdate(pref_service_, iter->first).Get());
  }
}

std::unique_ptr<base::Value>
DataReductionProxyCompressionStats::HistoricNetworkStatsInfoToValue() {
  DCHECK(thread_checker_.CalledOnValidThread());
  int64_t total_received = GetInt64(prefs::kHttpReceivedContentLength);
  int64_t total_original = GetInt64(prefs::kHttpOriginalContentLength);

  auto dict = std::make_unique<base::DictionaryValue>();
  // Use strings to avoid overflow. base::Value only supports 32-bit integers.
  dict->SetString("historic_received_content_length",
                  base::Int64ToString(total_received));
  dict->SetString("historic_original_content_length",
                  base::Int64ToString(total_original));
  return std::move(dict);
}

std::unique_ptr<base::Value>
DataReductionProxyCompressionStats::SessionNetworkStatsInfoToValue() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto dict = std::make_unique<base::DictionaryValue>();
  // Use strings to avoid overflow. base::Value only supports 32-bit integers.
  dict->SetString("session_received_content_length",
                  base::Int64ToString(session_total_received_));
  dict->SetString("session_original_content_length",
                  base::Int64ToString(session_total_original_));
  return std::move(dict);
}

int64_t DataReductionProxyCompressionStats::GetLastUpdateTime() {
  int64_t last_update_internal =
      GetInt64(prefs::kDailyHttpContentLengthLastUpdateDate);
  base::Time last_update = base::Time::FromInternalValue(last_update_internal);
  return static_cast<int64_t>(last_update.ToJsTime());
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

int64_t DataReductionProxyCompressionStats::GetHttpReceivedContentLength() {
  return GetInt64(prefs::kHttpReceivedContentLength);
}

int64_t DataReductionProxyCompressionStats::GetHttpOriginalContentLength() {
  return GetInt64(prefs::kHttpOriginalContentLength);
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
    int64_t* original_content_length,
    int64_t* received_content_length,
    int64_t* last_update_time) {
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

  int64_t orig = 0L;
  int64_t recv = 0L;
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

void DataReductionProxyCompressionStats::GetHistoricalDataUsage(
    const HistoricalDataUsageCallback& get_data_usage_callback) {
  GetHistoricalDataUsageImpl(get_data_usage_callback, base::Time::Now());
}

void DataReductionProxyCompressionStats::DeleteBrowsingHistory(
    const base::Time& start,
    const base::Time& end) {
  DCHECK_NE(LOADING, current_data_usage_load_status_);

  if (!data_usage_map_last_updated_.is_null() &&
      DataUsageStore::BucketOverlapsInterval(data_usage_map_last_updated_,
                                             start, end)) {
    data_usage_map_.clear();
    data_usage_map_last_updated_ = base::Time();
    data_usage_map_is_dirty_ = false;
  }

  service_->DeleteBrowsingHistory(start, end);
}

void DataReductionProxyCompressionStats::OnCurrentDataUsageLoaded(
    std::unique_ptr<DataUsageBucket> data_usage) {
  // Exit early if the pref was turned off before loading from storage
  // completed.
  if (!data_usage_reporting_enabled_.GetValue()) {
    DCHECK_EQ(NOT_LOADED, current_data_usage_load_status_);
    DCHECK(data_usage_map_.empty());
    current_data_usage_load_status_ = NOT_LOADED;
    return;
  } else {
    DCHECK_EQ(LOADING, current_data_usage_load_status_);
  }

  DCHECK(data_usage_map_last_updated_.is_null());
  DCHECK(data_usage_map_.empty());

  // We currently do not break down by connection type. However, we use a schema
  // that makes it easy to transition to a connection based breakdown without
  // requiring a data migration.
  DCHECK(data_usage->connection_usage_size() == 0 ||
         data_usage->connection_usage_size() == 1);
  for (const auto& connection_usage : data_usage->connection_usage()) {
    for (const auto& site_usage : connection_usage.site_usage()) {
      data_usage_map_[site_usage.hostname()] =
          std::make_unique<PerSiteDataUsage>(site_usage);
    }
  }

  data_usage_map_last_updated_ =
      base::Time::FromInternalValue(data_usage->last_updated_timestamp());
  current_data_usage_load_status_ = LOADED;
}

void DataReductionProxyCompressionStats::SetDataUsageReportingEnabled(
    bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (data_usage_reporting_enabled_.GetValue() != enabled) {
    data_usage_reporting_enabled_.SetValue(enabled);
    OnDataUsageReportingPrefChanged();
  }
}

void DataReductionProxyCompressionStats::ClearDataSavingStatistics() {
  DeleteHistoricalDataUsage();

  pref_service_->ClearPref(prefs::kDailyHttpContentLengthLastUpdateDate);
  pref_service_->ClearPref(prefs::kHttpReceivedContentLength);
  pref_service_->ClearPref(prefs::kHttpOriginalContentLength);

  pref_service_->ClearPref(prefs::kDailyHttpOriginalContentLengthApplication);
  pref_service_->ClearPref(prefs::kDailyHttpOriginalContentLengthVideo);
  pref_service_->ClearPref(prefs::kDailyHttpOriginalContentLengthUnknown);
  pref_service_->ClearPref(prefs::kDailyHttpReceivedContentLengthApplication);
  pref_service_->ClearPref(prefs::kDailyHttpReceivedContentLengthVideo);
  pref_service_->ClearPref(prefs::kDailyHttpReceivedContentLengthUnknown);

  pref_service_->ClearPref(
      prefs::kDailyOriginalContentLengthViaDataReductionProxyApplication);
  pref_service_->ClearPref(
      prefs::kDailyOriginalContentLengthViaDataReductionProxyVideo);
  pref_service_->ClearPref(
      prefs::kDailyOriginalContentLengthViaDataReductionProxyUnknown);
  pref_service_->ClearPref(
      prefs::kDailyContentLengthViaDataReductionProxyApplication);
  pref_service_->ClearPref(
      prefs::kDailyContentLengthViaDataReductionProxyVideo);
  pref_service_->ClearPref(
      prefs::kDailyContentLengthViaDataReductionProxyUnknown);

  pref_service_->ClearPref(
      prefs::
          kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication);
  pref_service_->ClearPref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo);
  pref_service_->ClearPref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown);
  pref_service_->ClearPref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabledApplication);
  pref_service_->ClearPref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabledVideo);
  pref_service_->ClearPref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabledUnknown);

  pref_service_->ClearPref(
      prefs::kDailyContentLengthHttpsWithDataReductionProxyEnabled);
  pref_service_->ClearPref(
      prefs::kDailyContentLengthLongBypassWithDataReductionProxyEnabled);
  pref_service_->ClearPref(
      prefs::kDailyContentLengthShortBypassWithDataReductionProxyEnabled);
  pref_service_->ClearPref(
      prefs::kDailyContentLengthUnknownWithDataReductionProxyEnabled);
  pref_service_->ClearPref(prefs::kDailyContentLengthViaDataReductionProxy);
  pref_service_->ClearPref(
      prefs::kDailyContentLengthWithDataReductionProxyEnabled);
  pref_service_->ClearPref(prefs::kDailyHttpOriginalContentLength);
  pref_service_->ClearPref(prefs::kDailyHttpReceivedContentLength);
  pref_service_->ClearPref(
      prefs::kDailyOriginalContentLengthViaDataReductionProxy);
  pref_service_->ClearPref(
      prefs::kDailyOriginalContentLengthWithDataReductionProxyEnabled);

  for (auto iter = list_pref_map_.begin(); iter != list_pref_map_.end();
       ++iter) {
    iter->second->Clear();
  }
}

void DataReductionProxyCompressionStats::DelayedWritePrefs() {
  if (pref_writer_timer_.IsRunning())
    return;

  pref_writer_timer_.Start(FROM_HERE, delay_, this,
                           &DataReductionProxyCompressionStats::WritePrefs);
}

void DataReductionProxyCompressionStats::TransferList(
    const base::ListValue& from_list,
    base::ListValue* to_list) {
  to_list->Clear();
  from_list.CreateDeepCopy()->Swap(to_list);
}

void DataReductionProxyCompressionStats::RecordRequestSizePrefs(
    int64_t data_used,
    int64_t original_size,
    bool with_data_saver_enabled,
    DataReductionProxyRequestType request_type,
    const std::string& mime_type,
    const base::Time& now) {
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
  int64_t then_internal = GetInt64(
      data_reduction_proxy::prefs::kDailyHttpContentLengthLastUpdateDate);

  // Local midnight could have been shifted due to time zone change.
  // If time is null then don't care if midnight will be wrong shifted due to
  // time zone change because it's still too much time ago.
  base::Time then_midnight = base::Time::FromInternalValue(then_internal);
  if (!then_midnight.is_null()) {
    then_midnight = then_midnight.LocalMidnight();
  }
  base::Time midnight = now.LocalMidnight();

  DailyDataSavingUpdate total(
      this, data_reduction_proxy::prefs::kDailyHttpOriginalContentLength,
      data_reduction_proxy::prefs::kDailyHttpReceivedContentLength);
  DailyDataSavingUpdate proxy_enabled(
      this, data_reduction_proxy::prefs::
                kDailyOriginalContentLengthWithDataReductionProxyEnabled,
      data_reduction_proxy::prefs::
          kDailyContentLengthWithDataReductionProxyEnabled);
  DailyDataSavingUpdate via_proxy(
      this, data_reduction_proxy::prefs::
                kDailyOriginalContentLengthViaDataReductionProxy,
      data_reduction_proxy::prefs::kDailyContentLengthViaDataReductionProxy);
  DailyContentLengthUpdate https(
      this, data_reduction_proxy::prefs::
                kDailyContentLengthHttpsWithDataReductionProxyEnabled);
  DailyContentLengthUpdate short_bypass(
      this, data_reduction_proxy::prefs::
                kDailyContentLengthShortBypassWithDataReductionProxyEnabled);
  DailyContentLengthUpdate long_bypass(
      this, data_reduction_proxy::prefs::
                kDailyContentLengthLongBypassWithDataReductionProxyEnabled);
  DailyContentLengthUpdate unknown(
      this, data_reduction_proxy::prefs::
                kDailyContentLengthUnknownWithDataReductionProxyEnabled);

  int days_since_last_update = (midnight - then_midnight).InDays();
  if (days_since_last_update) {
    // Record the last update time in microseconds in UTC.
    SetInt64(data_reduction_proxy::prefs::kDailyHttpContentLengthLastUpdateDate,
             midnight.ToInternalValue());

    // A new day. Report the previous day's data if exists. We'll lose usage
    // data if the last time Chrome was run was more than a day ago.
    // Here, we prefer collecting less data but the collected data is
    // associated with an accurate date.
    if (days_since_last_update == 1) {
      RecordDailyContentLengthHistograms(
          total.GetOriginalListPrefValue(kNumDaysInHistory - 1),
          total.GetReceivedListPrefValue(kNumDaysInHistory - 1),
          proxy_enabled.GetOriginalListPrefValue(kNumDaysInHistory - 1),
          proxy_enabled.GetReceivedListPrefValue(kNumDaysInHistory - 1),
          via_proxy.GetOriginalListPrefValue(kNumDaysInHistory - 1),
          via_proxy.GetReceivedListPrefValue(kNumDaysInHistory - 1),
          https.GetListPrefValue(kNumDaysInHistory - 1),
          short_bypass.GetListPrefValue(kNumDaysInHistory - 1),
          long_bypass.GetListPrefValue(kNumDaysInHistory - 1),
          unknown.GetListPrefValue(kNumDaysInHistory - 1));

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyHttpOriginalContentLengthApplication,
          "Net.DailyOriginalContentLength_Application");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyHttpReceivedContentLengthApplication,
          "Net.DailyReceivedContentLength_Application");

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthVideo,
          "Net.DailyOriginalContentLength_Video");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthVideo,
          "Net.DailyContentLength_Video");

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthUnknown,
          "Net.DailyOriginalContentLength_UnknownMime");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthUnknown,
          "Net.DailyContentLength_UnknownMime");

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication,
          "Net.DailyOriginalContentLength_DataReductionProxyEnabled_"
          "Application");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyContentLengthWithDataReductionProxyEnabledApplication,
          "Net.DailyContentLength_DataReductionProxyEnabled_Application");

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo,
          "Net.DailyOriginalContentLength_DataReductionProxyEnabled_Video");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyContentLengthWithDataReductionProxyEnabledVideo,
          "Net.DailyContentLength_DataReductionProxyEnabled_Video");
      int64_t original_length_with_data_reduction_enabled_video = GetInt64(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo);
      if (original_length_with_data_reduction_enabled_video > 0) {
        int64_t received_length_with_data_reduction_enabled_video =
            GetInt64(data_reduction_proxy::prefs::
                         kDailyContentLengthWithDataReductionProxyEnabledVideo);
        int percent_data_reduction_proxy_enabled_video = 0;
        // UMA percentage cannot be negative.
        // The DataReductionProxy server will only serve optimized video content
        // if the optimized content is smaller than the original content.
        // TODO(ryansturm): Track daily data inflation percents here and
        // elsewhere. http://crbug.com/595818
        if (original_length_with_data_reduction_enabled_video >
            received_length_with_data_reduction_enabled_video) {
          percent_data_reduction_proxy_enabled_video =
              100 * (original_length_with_data_reduction_enabled_video -
                     received_length_with_data_reduction_enabled_video) /
              original_length_with_data_reduction_enabled_video;
        }
        UMA_HISTOGRAM_PERCENTAGE(
            "Net.DailyContentSavingPercent_DataReductionProxyEnabled_Video",
            percent_data_reduction_proxy_enabled_video);
      }

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown,
          "Net.DailyOriginalContentLength_DataReductionProxyEnabled_"
          "UnknownMime");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyContentLengthWithDataReductionProxyEnabledUnknown,
          "Net.DailyContentLength_DataReductionProxyEnabled_UnknownMime")

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthViaDataReductionProxyApplication,
          "Net.DailyOriginalContentLength_ViaDataReductionProxy_Application");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyContentLengthViaDataReductionProxyApplication,
          "Net.DailyContentLength_ViaDataReductionProxy_Application");

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthViaDataReductionProxyVideo,
          "Net.DailyOriginalContentLength_ViaDataReductionProxy_Video");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyContentLengthViaDataReductionProxyVideo,
          "Net.DailyContentLength_ViaDataReductionProxy_Video");
      int64_t original_length_via_data_reduction_proxy_video =
          GetInt64(data_reduction_proxy::prefs::
                       kDailyOriginalContentLengthViaDataReductionProxyVideo);
      if (original_length_via_data_reduction_proxy_video > 0) {
        int64_t received_length_via_data_reduction_proxy_video =
            GetInt64(data_reduction_proxy::prefs::
                         kDailyContentLengthViaDataReductionProxyVideo);
        int percent_via_data_reduction_proxy_video = 0;
        // UMA percentage cannot be negative.
        // The DataReductionProxy server will only serve optimized video content
        // if the optimized content is smaller than the original content.
        // TODO(ryansturm): Track daily data inflation percents here and
        // elsewhere. http://crbug.com/595818
        if (original_length_via_data_reduction_proxy_video >
            received_length_via_data_reduction_proxy_video) {
          percent_via_data_reduction_proxy_video =
              100 * (original_length_via_data_reduction_proxy_video -
                     received_length_via_data_reduction_proxy_video) /
              original_length_via_data_reduction_proxy_video;
        }
        UMA_HISTOGRAM_PERCENTAGE(
            "Net.DailyContentSavingPercent_ViaDataReductionProxy_Video",
            percent_via_data_reduction_proxy_video);
      }

      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthViaDataReductionProxyUnknown,
          "Net.DailyOriginalContentLength_ViaDataReductionProxy_UnknownMime");
      RECORD_INT64PREF_TO_HISTOGRAM(
          data_reduction_proxy::prefs::
              kDailyContentLengthViaDataReductionProxyUnknown,
          "Net.DailyContentLength_ViaDataReductionProxy_UnknownMime");
    }

    RecordSavingsClearedNegativeClockMetric(days_since_last_update);

    // The system may go backwards in time by up to a day for legitimate
    // reasons, such as with changes to the time zone. In such cases, we
    // keep adding to the current day which is why we check for
    // |days_since_last_update != -1|.
    // Note: we accept the fact that some reported data is shifted to
    // the adjacent day if users travel back and forth across time zones.
    if (days_since_last_update && (days_since_last_update != -1)) {
      if (days_since_last_update < -1) {
        pref_service_->SetInt64(
            prefs::kDataReductionProxySavingsClearedNegativeSystemClock,
            now.ToInternalValue());
      }
      SetInt64(data_reduction_proxy::prefs::
                   kDailyHttpOriginalContentLengthApplication,
               0);
      SetInt64(data_reduction_proxy::prefs::
                   kDailyHttpReceivedContentLengthApplication,
               0);

      SetInt64(
          data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthVideo, 0);
      SetInt64(
          data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthVideo, 0);

      SetInt64(
          data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthUnknown,
          0);
      SetInt64(
          data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthUnknown,
          0);

      SetInt64(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication,
          0);
      SetInt64(data_reduction_proxy::prefs::
                   kDailyContentLengthWithDataReductionProxyEnabledApplication,
               0);

      SetInt64(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo,
          0);
      SetInt64(data_reduction_proxy::prefs::
                   kDailyContentLengthWithDataReductionProxyEnabledVideo,
               0);

      SetInt64(
          data_reduction_proxy::prefs::
              kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown,
          0);
      SetInt64(data_reduction_proxy::prefs::
                   kDailyContentLengthWithDataReductionProxyEnabledUnknown,
               0);

      SetInt64(data_reduction_proxy::prefs::
                   kDailyOriginalContentLengthViaDataReductionProxyApplication,
               0);
      SetInt64(data_reduction_proxy::prefs::
                   kDailyContentLengthViaDataReductionProxyApplication,
               0);

      SetInt64(data_reduction_proxy::prefs::
                   kDailyOriginalContentLengthViaDataReductionProxyVideo,
               0);
      SetInt64(data_reduction_proxy::prefs::
                   kDailyContentLengthViaDataReductionProxyVideo,
               0);

      SetInt64(data_reduction_proxy::prefs::
                   kDailyOriginalContentLengthViaDataReductionProxyUnknown,
               0);
      SetInt64(data_reduction_proxy::prefs::
                   kDailyContentLengthViaDataReductionProxyUnknown,
               0);
    }
  }

  total.UpdateForDateChange(days_since_last_update);
  proxy_enabled.UpdateForDateChange(days_since_last_update);
  via_proxy.UpdateForDateChange(days_since_last_update);
  https.UpdateForDateChange(days_since_last_update);
  short_bypass.UpdateForDateChange(days_since_last_update);
  long_bypass.UpdateForDateChange(days_since_last_update);
  unknown.UpdateForDateChange(days_since_last_update);

  total.Add(original_size, data_used);
  if (with_data_saver_enabled) {
    proxy_enabled.Add(original_size, data_used);
    // Ignore data source cases, if exist, when
    // "with_data_saver_enabled == false"
    switch (request_type) {
      case VIA_DATA_REDUCTION_PROXY:
        via_proxy.Add(original_size, data_used);
        break;
      case HTTPS:
        https.Add(data_used);
        break;
      case SHORT_BYPASS:
        short_bypass.Add(data_used);
        break;
      case LONG_BYPASS:
        long_bypass.Add(data_used);
        break;
      case UPDATE:
      case DIRECT_HTTP:
        // Don't record any request level prefs. If this is an update, this data
        // was already recorded at the URLRequest level. Updates are generally
        // page load level optimizations and don't correspond to request types.
        return;
      case UNKNOWN_TYPE:
        unknown.Add(data_used);
        break;
      default:
        NOTREACHED();
    }
  }

  bool via_data_reduction_proxy = request_type == VIA_DATA_REDUCTION_PROXY;
  bool is_application = net::MatchesMimeType("application/*", mime_type);
  bool is_video = net::MatchesMimeType("video/*", mime_type);
  bool is_mime_type_empty = mime_type.empty();
  if (is_application) {
    IncrementDailyUmaPrefs(
        original_size, data_used,
        data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthApplication,
        data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthApplication,
        with_data_saver_enabled,
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthWithDataReductionProxyEnabledApplication,
        data_reduction_proxy::prefs::
            kDailyContentLengthWithDataReductionProxyEnabledApplication,
        via_data_reduction_proxy,
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthViaDataReductionProxyApplication,
        data_reduction_proxy::prefs::
            kDailyContentLengthViaDataReductionProxyApplication);
  } else if (is_video) {
    IncrementDailyUmaPrefs(
        original_size, data_used,
        data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthVideo,
        data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthVideo,
        with_data_saver_enabled,
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthWithDataReductionProxyEnabledVideo,
        data_reduction_proxy::prefs::
            kDailyContentLengthWithDataReductionProxyEnabledVideo,
        via_data_reduction_proxy,
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthViaDataReductionProxyVideo,
        data_reduction_proxy::prefs::
            kDailyContentLengthViaDataReductionProxyVideo);
  } else if (is_mime_type_empty) {
    IncrementDailyUmaPrefs(
        original_size, data_used,
        data_reduction_proxy::prefs::kDailyHttpOriginalContentLengthUnknown,
        data_reduction_proxy::prefs::kDailyHttpReceivedContentLengthUnknown,
        with_data_saver_enabled,
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthWithDataReductionProxyEnabledUnknown,
        data_reduction_proxy::prefs::
            kDailyContentLengthWithDataReductionProxyEnabledUnknown,
        via_data_reduction_proxy,
        data_reduction_proxy::prefs::
            kDailyOriginalContentLengthViaDataReductionProxyUnknown,
        data_reduction_proxy::prefs::
            kDailyContentLengthViaDataReductionProxyUnknown);
  }
}

void DataReductionProxyCompressionStats::IncrementDailyUmaPrefs(
    int64_t original_size,
    int64_t received_size,
    const char* original_size_pref,
    const char* received_size_pref,
    bool data_reduction_proxy_enabled,
    const char* original_size_with_proxy_enabled_pref,
    const char* recevied_size_with_proxy_enabled_pref,
    bool via_data_reduction_proxy,
    const char* original_size_via_proxy_pref,
    const char* received_size_via_proxy_pref) {
  IncreaseInt64Pref(original_size_pref, original_size);
  IncreaseInt64Pref(received_size_pref, received_size);

  if (data_reduction_proxy_enabled) {
    IncreaseInt64Pref(original_size_with_proxy_enabled_pref, original_size);
    IncreaseInt64Pref(recevied_size_with_proxy_enabled_pref, received_size);
  }

  if (via_data_reduction_proxy) {
    IncreaseInt64Pref(original_size_via_proxy_pref, original_size);
    IncreaseInt64Pref(received_size_via_proxy_pref, received_size);
  }
}

void DataReductionProxyCompressionStats::RecordDataUseByHost(
    const std::string& data_usage_host,
    int64_t data_used,
    int64_t original_size,
    const base::Time time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (current_data_usage_load_status_ != LOADED)
    return;

  DCHECK(data_usage_reporting_enabled_.GetValue());

  if (!DataUsageStore::AreInSameInterval(data_usage_map_last_updated_, time)) {
    PersistDataUsage();
    data_usage_map_.clear();
    data_usage_map_last_updated_ = base::Time();
  }

  std::string normalized_host = NormalizeHostname(data_usage_host);
  auto j = data_usage_map_.insert(
      std::make_pair(normalized_host, std::make_unique<PerSiteDataUsage>()));
  PerSiteDataUsage* per_site_usage = j.first->second.get();
  per_site_usage->set_hostname(normalized_host);
  per_site_usage->set_original_size(per_site_usage->original_size() +
                                    original_size);
  per_site_usage->set_data_used(per_site_usage->data_used() + data_used);

  data_usage_map_last_updated_ = time;
  data_usage_map_is_dirty_ = true;
}

void DataReductionProxyCompressionStats::PersistDataUsage() {
  DCHECK(current_data_usage_load_status_ == LOADED);

  if (data_usage_map_is_dirty_) {
    std::unique_ptr<DataUsageBucket> data_usage_bucket(new DataUsageBucket());
    data_usage_bucket->set_last_updated_timestamp(
        data_usage_map_last_updated_.ToInternalValue());
    PerConnectionDataUsage* connection_usage =
        data_usage_bucket->add_connection_usage();
    for (auto i = data_usage_map_.begin(); i != data_usage_map_.end(); ++i) {
        PerSiteDataUsage* per_site_usage = connection_usage->add_site_usage();
        per_site_usage->CopyFrom(*(i->second.get()));
    }
    service_->StoreCurrentDataUsageBucket(std::move(data_usage_bucket));
  }

  data_usage_map_is_dirty_ = false;
}

void DataReductionProxyCompressionStats::DeleteHistoricalDataUsage() {
  // This method does not support being called in |LOADING| status since this
  // means that the in-memory data usage will get populated when data usage
  // loads, which will undo the clear below. This method is called when users
  // click on the "Clear Data" button, or when user deletes the extension. In
  // both cases, enough time has passed since startup to load current data
  // usage. Technically, this could occur, and will have the effect of not
  // clearing data from the current bucket.
  // TODO(kundaji): Use cancellable tasks and remove this DCHECK.
  DCHECK(current_data_usage_load_status_ != LOADING);

  data_usage_map_.clear();
  data_usage_map_last_updated_ = base::Time();
  data_usage_map_is_dirty_ = false;

  service_->DeleteHistoricalDataUsage();
}

void DataReductionProxyCompressionStats::GetHistoricalDataUsageImpl(
    const HistoricalDataUsageCallback& get_data_usage_callback,
    const base::Time& now) {
#if !defined(OS_ANDROID)
  if (current_data_usage_load_status_ != LOADED) {
    // If current data usage has not yet loaded, we return an empty array. The
    // extension can retry after a slight delay.
    // This use case is unlikely to occur in practice since current data usage
    // should have sufficient time to load before user tries to view data usage.
    get_data_usage_callback.Run(
        std::make_unique<std::vector<DataUsageBucket>>());
    return;
  }
#endif

  if (current_data_usage_load_status_ == LOADED)
    PersistDataUsage();

  if (!data_usage_map_last_updated_.is_null() &&
      !DataUsageStore::AreInSameInterval(data_usage_map_last_updated_, now)) {
    data_usage_map_.clear();
    data_usage_map_last_updated_ = base::Time();

    // Force the last bucket to be for the current interval.
    std::unique_ptr<DataUsageBucket> data_usage_bucket(new DataUsageBucket());
    data_usage_bucket->set_last_updated_timestamp(now.ToInternalValue());
    service_->StoreCurrentDataUsageBucket(std::move(data_usage_bucket));
  }

  service_->LoadHistoricalDataUsage(get_data_usage_callback);
}

void DataReductionProxyCompressionStats::OnDataUsageReportingPrefChanged() {
  if (data_usage_reporting_enabled_.GetValue()) {
    if (current_data_usage_load_status_ == NOT_LOADED) {
      current_data_usage_load_status_ = LOADING;
      service_->LoadCurrentDataUsageBucket(base::Bind(
          &DataReductionProxyCompressionStats::OnCurrentDataUsageLoaded,
          weak_factory_.GetWeakPtr()));
    }
  } else {
// Don't delete the historical data on Android, but clear the map.
#if defined(OS_ANDROID)
    if (current_data_usage_load_status_ == LOADED)
      PersistDataUsage();

    data_usage_map_.clear();
    data_usage_map_last_updated_ = base::Time();
    data_usage_map_is_dirty_ = false;
#else
    DeleteHistoricalDataUsage();
#endif
    current_data_usage_load_status_ = NOT_LOADED;
  }
}

// static
std::string DataReductionProxyCompressionStats::NormalizeHostname(
    const std::string& host) {
  size_t pos = host.find("://");
  if (pos != std::string::npos)
    return host.substr(pos + 3);

  return host;
}

}  // namespace data_reduction_proxy
