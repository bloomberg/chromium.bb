// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_COMPRESSION_STATS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_COMPRESSION_STATS_H_

#include <map>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"

class PrefChangeRegistrar;
class PrefService;

namespace base {
class ListValue;
class SequencedTaskRunner;
class Value;
}

namespace data_reduction_proxy {

// Data reduction proxy delayed pref service reduces the number calls to pref
// service by storing prefs in memory and writing to the given PrefService after
// |delay| amount of time. If |delay| is zero, the delayed pref service writes
// directly to the PrefService and does not store the prefs in memory. All
// prefs must be stored and read on the UI thread.
class DataReductionProxyCompressionStats {
 public:
  // Constructs a data reduction proxy delayed pref service object using
  // |pref_service|. Writes prefs to |pref_service| after |delay|
  // and stores them in |pref_map_| and |list_pref_map| between writes.
  // If |delay| is zero, writes directly to the PrefService and does not store
  // in the maps.
  DataReductionProxyCompressionStats(
      PrefService* pref_service,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::TimeDelta& delay);
  ~DataReductionProxyCompressionStats();

  // Records daily data savings statistics to prefs and reports data savings
  // UMA.
  void UpdateContentLengths(int64 received_content_length,
                            int64 original_content_length,
                            bool data_reduction_proxy_enabled,
                            DataReductionProxyRequestType request_type);

  // Creates a |Value| summary of the persistent state of the network session.
  // The caller is responsible for deleting the returned value.
  // Must be called on the UI thread.
  base::Value* HistoricNetworkStatsInfoToValue();

  // Returns the time in milliseconds since epoch that the last update was made
  // to the daily original and received content lengths.
  int64 GetLastUpdateTime();

  // Resets daily content length statistics.
  void ResetStatistics();

  // Clears all data saving statistics.
  void ClearDataSavingStatistics();

  // Returns a list of all the daily content lengths.
  ContentLengthList GetDailyContentLengths(const char* pref_name);

  // Returns aggregate received and original content lengths over the specified
  // number of days, as well as the time these stats were last updated.
  void GetContentLengths(unsigned int days,
                         int64* original_content_length,
                         int64* received_content_length,
                         int64* last_update_time);

 private:
  friend class DataReductionProxyCompressionStatsTest;

  typedef std::map<const char*, int64> DataReductionProxyPrefMap;
  typedef base::ScopedPtrHashMap<const char*, scoped_ptr<base::ListValue>>
      DataReductionProxyListPrefMap;

  // Loads all data_reduction_proxy::prefs into the |pref_map_| and
  // |list_pref_map_|.
  void Init();

  // Gets the value of |pref| from the pref service and adds it to the
  // |pref_map|.
  void InitInt64Pref(const char* pref);

  // Gets the value of |pref| from the pref service and adds it to the
  // |list_pref_map|.
  void InitListPref(const char* pref);

  void OnUpdateContentLengths();

  // Gets the int64 pref at |pref_path| from the |DataReductionProxyPrefMap|.
  int64 GetInt64(const char* pref_path);

  // Updates the pref value in the |DataReductionProxyPrefMap| map.
  // The pref is later written to |pref service_|.
  void SetInt64(const char* pref_path, int64 pref_value);

  // Gets the pref list at |pref_path| from the |DataReductionProxyPrefMap|.
  base::ListValue* GetList(const char* pref_path);

  // Writes the prefs stored in |DataReductionProxyPrefMap| and
  // |DataReductionProxyListPrefMap| to |pref_service|.
  void WritePrefs();

  // Writes the stored prefs to |pref_service| and then posts another a delayed
  // task to write prefs again in |kMinutesBetweenWrites|.
  void DelayedWritePrefs();

  // Copies the values at each index of |from_list| to the same index in
  // |to_list|.
  void TransferList(const base::ListValue& from_list,
                    base::ListValue* to_list);

  // Gets an int64, stored as a string, in a ListPref at the specified
  // index.
  int64 GetListPrefInt64Value(const base::ListValue& list_update, size_t index);

  // Records content length updates to prefs.
  void RecordContentLengthPrefs(int64 received_content_length,
                                int64 original_content_length,
                                bool with_data_reduction_proxy_enabled,
                                DataReductionProxyRequestType request_type,
                                base::Time now);

  // Record UMA with data savings bytes and percent over the past
  // |DataReductionProxy::kNumDaysInHistorySummary| days. These numbers
  // are displayed to users as their data savings.
  void RecordUserVisibleDataSavings();

  PrefService* pref_service_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const base::TimeDelta delay_;
  bool delayed_task_posted_;
  DataReductionProxyPrefMap pref_map_;
  DataReductionProxyListPrefMap list_pref_map_;
  scoped_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<DataReductionProxyCompressionStats> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyCompressionStats);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_COMPRESSION_STATS_H_
