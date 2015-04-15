// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/single_thread_task_runner.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/log/net_log.h"

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
      shutdown_on_ui_(false),
      url_request_context_getter_(nullptr),
      weak_factory_(this) {
  DCHECK(net_log);
  DCHECK(io_task_runner_);
  DCHECK(ui_task_runner_);
  scoped_ptr<DataReductionProxyParams> params(
      new DataReductionProxyParams(param_flags));
  params->EnableQuic(enable_quic);
  event_store_.reset(new DataReductionProxyEventStore(ui_task_runner));
  configurator_.reset(
      new DataReductionProxyConfigurator(net_log, event_store_.get()));
  bool use_config_client = DataReductionProxyParams::IsConfigClientEnabled();
  DataReductionProxyMutableConfigValues* raw_mutable_config = nullptr;
  if (use_config_client) {
    scoped_ptr<DataReductionProxyMutableConfigValues> mutable_config =
        DataReductionProxyMutableConfigValues::CreateFromParams(params.get());
    raw_mutable_config = mutable_config.get();
    config_.reset(new DataReductionProxyConfig(
        io_task_runner_, net_log, mutable_config.Pass(), configurator_.get(),
        event_store_.get()));
  } else {
    config_.reset(
        new DataReductionProxyConfig(io_task_runner_, net_log, params.Pass(),
                                     configurator_.get(), event_store_.get()));
  }

  // It is safe to use base::Unretained here, since it gets executed
  // synchronously on the IO thread, and |this| outlives the caller (since the
  // caller is owned by |this|.
  bypass_stats_.reset(new DataReductionProxyBypassStats(
      config_.get(), base::Bind(&DataReductionProxyIOData::SetUnreachable,
                                base::Unretained(this))));
  request_options_.reset(
      new DataReductionProxyRequestOptions(client_, config_.get()));
  request_options_->Init();
  if (use_config_client) {
    config_client_.reset(new DataReductionProxyConfigServiceClient(
        params.Pass(), GetBackoffPolicy(), request_options_.get(),
        raw_mutable_config, config_.get()));
  }

  proxy_delegate_.reset(
      new DataReductionProxyDelegate(request_options_.get(), config_.get()));
 }

 DataReductionProxyIOData::DataReductionProxyIOData()
     : shutdown_on_ui_(false),
       url_request_context_getter_(nullptr),
       weak_factory_(this) {
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
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  service_ = data_reduction_proxy_service;
  url_request_context_getter_ = service_->url_request_context_getter();
  // Using base::Unretained is safe here, unless the browser is being shut down
  // before the Initialize task can be executed. The task is only created as
  // part of class initialization.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyIOData::InitializeOnIOThread,
                 base::Unretained(this)));
}

void DataReductionProxyIOData::InitializeOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  config_->InitializeOnIOThread(url_request_context_getter_);
  if (config_client_.get())
    config_client_->RetrieveConfig();
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyService::SetIOData,
                 service_, weak_factory_.GetWeakPtr()));
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
      config_.get(), bypass_stats_.get(), event_store_.get()));
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
  if (track_proxy_bypass_statistics)
    network_delegate->InitIODataAndUMA(this, &enabled_, bypass_stats_.get());
  return network_delegate.Pass();
}

void DataReductionProxyIOData::SetProxyPrefs(bool enabled,
                                             bool alternative_enabled,
                                             bool at_startup) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  config_->SetProxyConfig(enabled, alternative_enabled, at_startup);
}

void DataReductionProxyIOData::UpdateContentLengths(
    int64 received_content_length,
    int64 original_content_length,
    bool data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyService::UpdateContentLengths,
                 service_, received_content_length, original_content_length,
                 data_reduction_proxy_enabled, request_type));
}

void DataReductionProxyIOData::SetUnreachable(bool unreachable) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyService::SetUnreachable,
                 service_, unreachable));
}

}  // namespace data_reduction_proxy
