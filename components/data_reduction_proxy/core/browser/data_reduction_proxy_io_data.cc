// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_member.h"
#include "base/single_thread_task_runner.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_auth_request_handler.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
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
      statistics_prefs_(statistics_prefs.Pass()),
      settings_(settings),
      net_log_(net_log),
      io_task_runner_(io_task_runner),
      ui_task_runner_(ui_task_runner),
      shutdown_on_ui_(false) {
  DCHECK(settings);
  DCHECK(net_log);
  DCHECK(io_task_runner_.get());
  DCHECK(ui_task_runner_.get());
  params_ = settings->params()->Clone();
  auth_request_handler_.reset(new DataReductionProxyAuthRequestHandler(
      client_, params_.get(), io_task_runner_));
  event_store_.reset(new DataReductionProxyEventStore(ui_task_runner));
  configurator_.reset(new DataReductionProxyConfigurator(
      io_task_runner, net_log, event_store_.get()));
  proxy_delegate_.reset(
      new data_reduction_proxy::DataReductionProxyDelegate(
          auth_request_handler_.get(), params_.get()));
}

DataReductionProxyIOData::~DataReductionProxyIOData() {
  DCHECK(shutdown_on_ui_);
}

void DataReductionProxyIOData::InitOnUIThread(PrefService* pref_service) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  enabled_.Init(prefs::kDataReductionProxyEnabled, pref_service);
  enabled_.MoveToThread(io_task_runner_);
}

void  DataReductionProxyIOData::ShutdownOnUIThread() {
  DCHECK(!shutdown_on_ui_);
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (statistics_prefs_) {
    statistics_prefs_->WritePrefs();
    statistics_prefs_->ShutdownOnUIThread();
  }
  enabled_.Destroy();
  shutdown_on_ui_ = true;
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

void DataReductionProxyIOData::EnableCompressionStatisticsLogging(
    PrefService* prefs,
    const base::TimeDelta& commit_delay) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  statistics_prefs_.reset(
      new DataReductionProxyStatisticsPrefs(
          prefs, ui_task_runner_, commit_delay));
}

scoped_ptr<DataReductionProxyNetworkDelegate>
DataReductionProxyIOData::CreateNetworkDelegate(
    scoped_ptr<net::NetworkDelegate> wrapped_network_delegate,
    bool track_proxy_bypass_statistics) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  scoped_ptr<DataReductionProxyNetworkDelegate> network_delegate(
      new DataReductionProxyNetworkDelegate(
          wrapped_network_delegate.Pass(), params_.get(),
          auth_request_handler_.get(), configurator_.get()));
  if (track_proxy_bypass_statistics && !usage_stats_) {
    usage_stats_.reset(
        new data_reduction_proxy::DataReductionProxyUsageStats(
        params_.get(), settings_, ui_task_runner_));
    network_delegate->InitStatisticsPrefsAndUMA(
        ui_task_runner_, statistics_prefs_.get(), &enabled_,
        usage_stats_.get());
  }
  return network_delegate.Pass();
}

}  // namespace data_reduction_proxy
