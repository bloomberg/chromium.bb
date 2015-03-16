// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_member.h"
#include "base/single_thread_task_runner.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
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
    int param_flags,
    net::NetLog* net_log,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    bool enable_quic)
    : client_(client),
      net_log_(net_log),
      io_task_runner_(io_task_runner),
      ui_task_runner_(ui_task_runner),
      shutdown_on_ui_(false) {
  DCHECK(net_log);
  DCHECK(io_task_runner_.get());
  DCHECK(ui_task_runner_.get());
  scoped_ptr<DataReductionProxyParams> params(
      new DataReductionProxyParams(param_flags));
  event_store_.reset(new DataReductionProxyEventStore(ui_task_runner));
  configurator_.reset(new DataReductionProxyConfigurator(
      io_task_runner, net_log, event_store_.get()));
  config_.reset(new DataReductionProxyConfig(
      io_task_runner_, ui_task_runner_, net_log, params.Pass(),
      configurator_.get(), event_store_.get(), enable_quic));
  request_options_.reset(new DataReductionProxyRequestOptions(
      client_, config_.get(), io_task_runner_));
  request_options_->Init();
  proxy_delegate_.reset(
      new DataReductionProxyDelegate(request_options_.get(), config_.get()));
 }

DataReductionProxyIOData::DataReductionProxyIOData() : shutdown_on_ui_(false) {
}

DataReductionProxyIOData::~DataReductionProxyIOData() {
  DCHECK(shutdown_on_ui_);
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

void DataReductionProxyIOData::SetDataReductionProxyService(
    base::WeakPtr<DataReductionProxyService> data_reduction_proxy_service) {
  service_ = data_reduction_proxy_service;
  config()->SetDataReductionProxyService(data_reduction_proxy_service);
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
      config_.get(), usage_stats_.get(), event_store_.get()));
}

scoped_ptr<DataReductionProxyNetworkDelegate>
DataReductionProxyIOData::CreateNetworkDelegate(
    scoped_ptr<net::NetworkDelegate> wrapped_network_delegate,
    bool track_proxy_bypass_statistics) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  scoped_ptr<DataReductionProxyNetworkDelegate> network_delegate(
      new DataReductionProxyNetworkDelegate(
          wrapped_network_delegate.Pass(), config_.get(),
          request_options_.get(), configurator_.get()));
  if (track_proxy_bypass_statistics && !usage_stats_) {
    usage_stats_.reset(new DataReductionProxyUsageStats(
        config_.get(), base::Bind(&DataReductionProxyIOData::SetUnreachable,
                                  base::Unretained(this)), ui_task_runner_));
    network_delegate->InitIODataAndUMA(ui_task_runner_, this, &enabled_,
                                       usage_stats_.get());
  }
  return network_delegate.Pass();
}

void DataReductionProxyIOData::SetUnreachable(bool unreachable) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (service_)
    service_->settings()->SetUnreachable(unreachable);
}

}  // namespace data_reduction_proxy
