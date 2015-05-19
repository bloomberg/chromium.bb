// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/prefs/pref_service.h"
#include "base/sequenced_task_runner.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service_observer.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"

namespace data_reduction_proxy {

DataReductionProxyService::DataReductionProxyService(
    scoped_ptr<DataReductionProxyCompressionStats> compression_stats,
    DataReductionProxySettings* settings,
    PrefService* prefs,
    net::URLRequestContextGetter* request_context_getter,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : url_request_context_getter_(request_context_getter),
      settings_(settings),
      prefs_(prefs),
      io_task_runner_(io_task_runner),
      initialized_(false),
      weak_factory_(this) {
  DCHECK(settings);
  compression_stats_ = compression_stats.Pass();
  event_store_.reset(new DataReductionProxyEventStore());
}

DataReductionProxyService::~DataReductionProxyService() {
}

void DataReductionProxyService::SetIOData(
    base::WeakPtr<DataReductionProxyIOData> io_data) {
  DCHECK(CalledOnValidThread());
  io_data_ = io_data;
  initialized_ = true;
  FOR_EACH_OBSERVER(DataReductionProxyServiceObserver,
                    observer_list_, OnServiceInitialized());
}

void DataReductionProxyService::Shutdown() {
  DCHECK(CalledOnValidThread());
  weak_factory_.InvalidateWeakPtrs();
}

void DataReductionProxyService::EnableCompressionStatisticsLogging(
    PrefService* prefs,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    const base::TimeDelta& commit_delay) {
  DCHECK(CalledOnValidThread());
  DCHECK(!compression_stats_);
  DCHECK(!prefs_);
  prefs_ = prefs;
  compression_stats_.reset(new DataReductionProxyCompressionStats(
      prefs_, ui_task_runner, commit_delay));
}

void DataReductionProxyService::UpdateContentLengths(
    int64 received_content_length,
    int64 original_content_length,
    bool data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type) {
  DCHECK(CalledOnValidThread());
  if (compression_stats_) {
    compression_stats_->UpdateContentLengths(
        received_content_length, original_content_length,
        data_reduction_proxy_enabled, request_type);
  }
}

void DataReductionProxyService::AddEvent(scoped_ptr<base::Value> event) {
  DCHECK(CalledOnValidThread());
  event_store_->AddEvent(event.Pass());
}

void DataReductionProxyService::AddEnabledEvent(scoped_ptr<base::Value> event,
                                                bool enabled) {
  DCHECK(CalledOnValidThread());
  event_store_->AddEnabledEvent(event.Pass(), enabled);
}

void DataReductionProxyService::AddEventAndSecureProxyCheckState(
    scoped_ptr<base::Value> event,
    SecureProxyCheckState state) {
  DCHECK(CalledOnValidThread());
  event_store_->AddEventAndSecureProxyCheckState(event.Pass(), state);
}

void DataReductionProxyService::AddAndSetLastBypassEvent(
    scoped_ptr<base::Value> event,
    int64 expiration_ticks) {
  DCHECK(CalledOnValidThread());
  event_store_->AddAndSetLastBypassEvent(event.Pass(), expiration_ticks);
}

void DataReductionProxyService::SetUnreachable(bool unreachable) {
  DCHECK(CalledOnValidThread());
  settings_->SetUnreachable(unreachable);
}

void DataReductionProxyService::SetInt64Pref(const std::string& pref_path,
                                             int64 value) {
  if (prefs_)
    prefs_->SetInt64(pref_path, value);
}

void DataReductionProxyService::SetProxyPrefs(bool enabled,
                                              bool alternative_enabled,
                                              bool at_startup) {
  DCHECK(CalledOnValidThread());
  if (io_task_runner_->BelongsToCurrentThread()) {
    io_data_->SetProxyPrefs(enabled, alternative_enabled, at_startup);
    return;
  }
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyIOData::SetProxyPrefs,
                 io_data_, enabled, alternative_enabled, at_startup));
}

void DataReductionProxyService::RetrieveConfig() {
  DCHECK(CalledOnValidThread());
  if (io_task_runner_->BelongsToCurrentThread()) {
    io_data_->RetrieveConfig();
    return;
  }
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyIOData::RetrieveConfig, io_data_));
}

void DataReductionProxyService::AddObserver(
    DataReductionProxyServiceObserver* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void DataReductionProxyService::RemoveObserver(
    DataReductionProxyServiceObserver* observer) {
  DCHECK(CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

bool DataReductionProxyService::Initialized() const {
  DCHECK(CalledOnValidThread());
  return initialized_;
}

base::WeakPtr<DataReductionProxyService>
DataReductionProxyService::GetWeakPtr() {
  DCHECK(CalledOnValidThread());
  return weak_factory_.GetWeakPtr();
}

}  // namespace data_reduction_proxy
