// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"

namespace data_reduction_proxy {

DataReductionProxyStatisticsPrefs::DataReductionProxyStatisticsPrefs(
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

DataReductionProxyStatisticsPrefs::~DataReductionProxyStatisticsPrefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  WritePrefs();
  pref_change_registrar_->RemoveAll();
  weak_factory_.InvalidateWeakPtrs();
}

void DataReductionProxyStatisticsPrefs::Init() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (delay_ == base::TimeDelta())
    return;

  // Init all int64 prefs.
  InitInt64Pref(data_reduction_proxy::prefs::
                kDailyHttpContentLengthLastUpdateDate);
  InitInt64Pref(data_reduction_proxy::prefs::kHttpReceivedContentLength);
  InitInt64Pref(data_reduction_proxy::prefs::kHttpOriginalContentLength);

  // Init all list prefs.
  InitListPref(data_reduction_proxy::prefs::
               kDailyContentLengthHttpsWithDataReductionProxyEnabled);
  InitListPref(data_reduction_proxy::prefs::
               kDailyContentLengthLongBypassWithDataReductionProxyEnabled);
  InitListPref(data_reduction_proxy::prefs::
               kDailyContentLengthShortBypassWithDataReductionProxyEnabled);
  InitListPref(data_reduction_proxy::prefs::
               kDailyContentLengthUnknownWithDataReductionProxyEnabled);
  InitListPref(data_reduction_proxy::prefs::
               kDailyContentLengthViaDataReductionProxy);
  InitListPref(data_reduction_proxy::prefs::
               kDailyContentLengthWithDataReductionProxyEnabled);
  InitListPref(data_reduction_proxy::prefs::kDailyHttpOriginalContentLength);
  InitListPref(data_reduction_proxy::prefs::kDailyHttpReceivedContentLength);
  InitListPref(data_reduction_proxy::prefs::
               kDailyOriginalContentLengthViaDataReductionProxy);
  InitListPref(data_reduction_proxy::prefs::
               kDailyOriginalContentLengthWithDataReductionProxyEnabled);

  pref_change_registrar_->Init(pref_service_);
  pref_change_registrar_->Add(prefs::kUpdateDailyReceivedContentLengths,
      base::Bind(&DataReductionProxyStatisticsPrefs::OnUpdateContentLengths,
                 GetWeakPtr()));
}

void DataReductionProxyStatisticsPrefs::OnUpdateContentLengths() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!pref_service_->GetBoolean(prefs::kUpdateDailyReceivedContentLengths))
    return;

  WritePrefs();
  pref_service_->SetBoolean(prefs::kUpdateDailyReceivedContentLengths, false);
}

void DataReductionProxyStatisticsPrefs::InitInt64Pref(const char* pref) {
  int64 pref_value = pref_service_->GetInt64(pref);
  pref_map_[pref] = pref_value;
}

void DataReductionProxyStatisticsPrefs::InitListPref(const char* pref) {
  scoped_ptr<base::ListValue> pref_value = scoped_ptr<base::ListValue>(
      pref_service_->GetList(pref)->DeepCopy());
  list_pref_map_.add(pref, pref_value.Pass());
}

int64 DataReductionProxyStatisticsPrefs::GetInt64(const char* pref_path) {
  if (delay_ == base::TimeDelta())
    return pref_service_->GetInt64(pref_path);

  DataReductionProxyPrefMap::iterator iter = pref_map_.find(pref_path);
  return iter->second;
}

void DataReductionProxyStatisticsPrefs::SetInt64(const char* pref_path,
                                                 int64 pref_value) {
  if (delay_ == base::TimeDelta()) {
    pref_service_->SetInt64(pref_path, pref_value);
    return;
  }

  DelayedWritePrefs();
  pref_map_[pref_path] = pref_value;
}

base::ListValue* DataReductionProxyStatisticsPrefs::GetList(
    const char* pref_path) {
  if (delay_ == base::TimeDelta())
    return ListPrefUpdate(pref_service_, pref_path).Get();

  DelayedWritePrefs();
  return list_pref_map_.get(pref_path);
}

void DataReductionProxyStatisticsPrefs::WritePrefs() {
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
DataReductionProxyStatisticsPrefs::HistoricNetworkStatsInfoToValue() {
  DCHECK(thread_checker_.CalledOnValidThread());
  int64 total_received = GetInt64(
      data_reduction_proxy::prefs::kHttpReceivedContentLength);
  int64 total_original = GetInt64(
      data_reduction_proxy::prefs::kHttpOriginalContentLength);

  base::DictionaryValue* dict = new base::DictionaryValue();
  // Use strings to avoid overflow. base::Value only supports 32-bit integers.
  dict->SetString("historic_received_content_length",
                  base::Int64ToString(total_received));
  dict->SetString("historic_original_content_length",
                  base::Int64ToString(total_original));
  return dict;
}

void DataReductionProxyStatisticsPrefs::DelayedWritePrefs() {
  // Only write after the first time posting the task.
  if (delayed_task_posted_)
    return;

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyStatisticsPrefs::WritePrefs,
                 GetWeakPtr()),
                 delay_);

  delayed_task_posted_ = true;
}

void DataReductionProxyStatisticsPrefs::TransferList(
    const base::ListValue& from_list,
    base::ListValue* to_list) {
  to_list->Clear();
  for (size_t i = 0; i < from_list.GetSize(); ++i) {
    to_list->Set(i, new base::StringValue(base::Int64ToString(
        GetListPrefInt64Value(from_list, i))));
  }
}

int64 DataReductionProxyStatisticsPrefs::GetListPrefInt64Value(
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

base::WeakPtr<DataReductionProxyStatisticsPrefs>
DataReductionProxyStatisticsPrefs::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace data_reduction_proxy
