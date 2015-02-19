// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_member.h"
#include "base/single_thread_task_runner.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_usage_stats.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/net_log.h"

namespace data_reduction_proxy {

DataReductionProxyIOData::DataReductionProxyIOData(
    const Client& client,
    scoped_ptr<DataReductionProxyStatisticsPrefs> statistics_prefs,
    DataReductionProxySettings* settings,
    net::NetLog* net_log,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : client_(client),
      temporary_statistics_prefs_(statistics_prefs.Pass()),
      settings_(settings),
      net_log_(net_log),
      io_task_runner_(io_task_runner),
      ui_task_runner_(ui_task_runner),
      shutdown_on_ui_(false),
      initialized_(false),
      network_delegate_created_(false) {
  DCHECK(settings);
  DCHECK(net_log);
  DCHECK(io_task_runner_.get());
  DCHECK(ui_task_runner_.get());
  params_ = settings->params()->Clone();
  request_options_.reset(new DataReductionProxyRequestOptions(
      client_, params_.get(), io_task_runner_));
  event_store_.reset(new DataReductionProxyEventStore(ui_task_runner));
  configurator_.reset(new DataReductionProxyConfigurator(
      io_task_runner, net_log, event_store_.get()));
  proxy_delegate_.reset(
      new data_reduction_proxy::DataReductionProxyDelegate(
          request_options_.get(), params_.get()));
  if (temporary_statistics_prefs_)
    statistics_prefs_ = temporary_statistics_prefs_->GetWeakPtr();
}

DataReductionProxyIOData::DataReductionProxyIOData() : shutdown_on_ui_(false) {
}

DataReductionProxyIOData::~DataReductionProxyIOData() {
  DCHECK(shutdown_on_ui_);
}

void DataReductionProxyIOData::Init() {
  DCHECK(!initialized_);
  DCHECK(!network_delegate_created_);
  initialized_ = true;
  request_options_->Init();
}

void DataReductionProxyIOData::InitOnUIThread(PrefService* pref_service) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  enabled_.Init(prefs::kDataReductionProxyEnabled, pref_service);
  enabled_.MoveToThread(io_task_runner_);
}

void DataReductionProxyIOData::ShutdownOnUIThread() {
  DCHECK(!shutdown_on_ui_);
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  enabled_.Destroy();
  shutdown_on_ui_ = true;
}

void DataReductionProxyIOData::SetDataReductionProxyStatisticsPrefs(
    base::WeakPtr<DataReductionProxyStatisticsPrefs> statistics_prefs) {
  statistics_prefs_ = statistics_prefs;
}

scoped_ptr<DataReductionProxyStatisticsPrefs>
DataReductionProxyIOData::PassStatisticsPrefs() {
  return temporary_statistics_prefs_.Pass();
}

bool DataReductionProxyIOData::IsEnabled() const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return enabled_.GetValue() ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableDataReductionProxy);
}

scoped_ptr<net::URLRequestInterceptor>
DataReductionProxyIOData::CreateInterceptor() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return make_scoped_ptr(new DataReductionProxyInterceptor(
      params_.get(), usage_stats_.get(), event_store_.get()));
}

scoped_ptr<DataReductionProxyNetworkDelegate>
DataReductionProxyIOData::CreateNetworkDelegate(
    scoped_ptr<net::NetworkDelegate> wrapped_network_delegate,
    bool track_proxy_bypass_statistics) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  scoped_ptr<DataReductionProxyNetworkDelegate> network_delegate(
      new DataReductionProxyNetworkDelegate(
          wrapped_network_delegate.Pass(), params_.get(),
          request_options_.get(), configurator_.get()));
  if (track_proxy_bypass_statistics && !usage_stats_) {
    usage_stats_.reset(new data_reduction_proxy::DataReductionProxyUsageStats(
        params_.get(), settings_, ui_task_runner_));
    network_delegate->InitStatisticsPrefsAndUMA(
        ui_task_runner_, statistics_prefs_, &enabled_, usage_stats_.get());
  }
  network_delegate_created_ = true;
  return network_delegate.Pass();
}

}  // namespace data_reduction_proxy
