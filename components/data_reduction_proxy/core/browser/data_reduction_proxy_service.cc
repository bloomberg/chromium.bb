// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service_observer.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

namespace data_reduction_proxy {

DataReductionProxyService::DataReductionProxyService(
    scoped_ptr<DataReductionProxyCompressionStats> compression_stats,
    DataReductionProxySettings* settings,
    net::URLRequestContextGetter* request_context_getter,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : url_request_context_getter_(request_context_getter),
      settings_(settings),
      io_task_runner_(io_task_runner),
      initialized_(false),
      weak_factory_(this) {
  DCHECK(settings);
  compression_stats_ = compression_stats.Pass();
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
  compression_stats_.reset(new DataReductionProxyCompressionStats(
      prefs, ui_task_runner, commit_delay));
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

void DataReductionProxyService::SetUnreachable(bool unreachable) {
  DCHECK(CalledOnValidThread());
  settings_->SetUnreachable(unreachable);
}

void DataReductionProxyService::SetProxyPrefs(bool enabled,
                                              bool alternative_enabled,
                                              bool at_startup) {
  DCHECK(CalledOnValidThread());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyIOData::SetProxyPrefs,
                 io_data_, enabled, alternative_enabled, at_startup));
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

void DataReductionProxyService::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(source == fetcher_.get());
  net::URLRequestStatus status = source->GetStatus();

  std::string response;
  source->GetResponseAsString(&response);

  fetcher_callback_.Run(response, status);
}

net::URLFetcher* DataReductionProxyService::GetURLFetcherForSecureProxyCheck(
    const GURL& secure_proxy_check_url) {
  net::URLFetcher* fetcher = net::URLFetcher::Create(
      secure_proxy_check_url, net::URLFetcher::GET, this);
  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_PROXY);
  DCHECK(url_request_context_getter_);
  fetcher->SetRequestContext(url_request_context_getter_);
  // Configure max retries to be at most kMaxRetries times for 5xx errors.
  static const int kMaxRetries = 5;
  fetcher->SetMaxRetriesOn5xx(kMaxRetries);
  fetcher->SetAutomaticallyRetryOnNetworkChanges(kMaxRetries);
  // The secure proxy check should not be redirected. Since the secure proxy
  // check will inevitably fail if it gets redirected somewhere else (e.g. by a
  // captive portal), short circuit that by giving up on the secure proxy check
  // if it gets redirected.
  fetcher->SetStopOnRedirect(true);
  return fetcher;
}

void DataReductionProxyService::SecureProxyCheck(
    const GURL& secure_proxy_check_url,
    FetcherResponseCallback fetcher_callback) {
  DCHECK(CalledOnValidThread());
  net::URLFetcher* fetcher =
      GetURLFetcherForSecureProxyCheck(secure_proxy_check_url);
  if (!fetcher)
    return;
  fetcher_.reset(fetcher);
  fetcher_callback_ = fetcher_callback;
  fetcher_->Start();
}

}  // namespace data_reduction_proxy
