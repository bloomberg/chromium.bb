// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_interceptor.h"

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_usage_stats.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_job_manager.h"
#include "url/url_constants.h"

namespace data_reduction_proxy {

DataReductionProxyInterceptor::DataReductionProxyInterceptor(
    DataReductionProxyConfig* config,
    DataReductionProxyUsageStats* stats,
    DataReductionProxyEventStore* event_store)
    : usage_stats_(stats),
      bypass_protocol_(
          new DataReductionProxyBypassProtocol(config, event_store)) {
}

DataReductionProxyInterceptor::~DataReductionProxyInterceptor() {
}

net::URLRequestJob* DataReductionProxyInterceptor::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return nullptr;
}

net::URLRequestJob* DataReductionProxyInterceptor::MaybeInterceptRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const GURL& location) const {
  return MaybeInterceptResponseOrRedirect(request, network_delegate);
}

net::URLRequestJob* DataReductionProxyInterceptor::MaybeInterceptResponse(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return MaybeInterceptResponseOrRedirect(request, network_delegate);
}

net::URLRequestJob*
DataReductionProxyInterceptor::MaybeInterceptResponseOrRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  if (request->response_info().was_cached)
    return nullptr;
  DataReductionProxyBypassType bypass_type = BYPASS_EVENT_TYPE_MAX;
  bool should_retry = bypass_protocol_->MaybeBypassProxyAndPrepareToRetry(
      request, &bypass_type);
  if (usage_stats_ && bypass_type != BYPASS_EVENT_TYPE_MAX)
    usage_stats_->SetBypassType(bypass_type);
  if (!should_retry)
    return nullptr;
  // Returning non-NULL has the effect of restarting the request with the
  // supplied job.
  DCHECK(request->url().SchemeIs(url::kHttpScheme));
  return net::URLRequestJobManager::GetInstance()->CreateJob(
      request, network_delegate);
}

}  // namespace data_reduction_proxy
