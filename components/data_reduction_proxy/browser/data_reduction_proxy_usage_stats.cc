// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_usage_stats.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"

using base::MessageLoopProxy;
using net::HostPortPair;
using net::ProxyServer;
using net::NetworkChangeNotifier;

namespace data_reduction_proxy {

DataReductionProxyUsageStats::DataReductionProxyUsageStats(
    DataReductionProxyParams* params,
    MessageLoopProxy* ui_thread_proxy,
    MessageLoopProxy* io_thread_proxy)
    : data_reduction_proxy_params_(params),
      ui_thread_proxy_(ui_thread_proxy),
      io_thread_proxy_(io_thread_proxy),
      eligible_num_requests_through_proxy_(0),
      actual_num_requests_through_proxy_(0) {
  NetworkChangeNotifier::AddNetworkChangeObserver(this);
};

DataReductionProxyUsageStats::~DataReductionProxyUsageStats() {
  NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
};

void DataReductionProxyUsageStats::OnUrlRequestCompleted(
    const net::URLRequest* request, bool started) {
  DCHECK(io_thread_proxy_->BelongsToCurrentThread());

  if (request->status().status() == net::URLRequestStatus::SUCCESS) {
    if (data_reduction_proxy_params_->IsDataReductionProxyEligible(request)) {
      bool was_received_via_proxy =
          data_reduction_proxy_params_->WasDataReductionProxyUsed(
              request, NULL);
      ui_thread_proxy_->PostTask(FROM_HERE, base::Bind(
          &DataReductionProxyUsageStats::IncRequestCountsOnUiThread,
          base::Unretained(this), was_received_via_proxy));
    }
  }
}

/**
 * Determines if the data reduction proxy is currently unreachable. We keep
 * track of count of requests which go through data reduction proxy as well as
 * count of requests which are eligible to go through the proxy. If and only if
 * no requests go through proxy and some requests were eligible, we conclude
 * that the proxy is unreachable.
 *
 * Returns true if the data reduction proxy is unreachable.
 */
bool DataReductionProxyUsageStats::isDataReductionProxyUnreachable() {
  DCHECK(ui_thread_proxy_->BelongsToCurrentThread());

  return eligible_num_requests_through_proxy_ > 0 &&
      actual_num_requests_through_proxy_ == 0;
}

void DataReductionProxyUsageStats::OnNetworkChanged(
    NetworkChangeNotifier::ConnectionType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ui_thread_proxy_->PostTask(FROM_HERE, base::Bind(
      &DataReductionProxyUsageStats::ClearRequestCountsOnUiThread,
      base::Unretained(this)));
}

void DataReductionProxyUsageStats::IncRequestCountsOnUiThread(
    bool was_received_via_proxy) {
  DCHECK(ui_thread_proxy_->BelongsToCurrentThread());
  if (was_received_via_proxy) {
    actual_num_requests_through_proxy_++;
  }
  eligible_num_requests_through_proxy_++;

  // To account for the case when the proxy works for a little while and then
  // gets blocked, we reset the counts occasionally.
  if (eligible_num_requests_through_proxy_ > 50
      && actual_num_requests_through_proxy_ > 0) {
    ClearRequestCountsOnUiThread();
  }
}

void DataReductionProxyUsageStats::ClearRequestCountsOnUiThread() {
  DCHECK(ui_thread_proxy_->BelongsToCurrentThread());
  eligible_num_requests_through_proxy_ = 0;
  actual_num_requests_through_proxy_ = 0;
}

}  // namespace data_reduction_proxy


