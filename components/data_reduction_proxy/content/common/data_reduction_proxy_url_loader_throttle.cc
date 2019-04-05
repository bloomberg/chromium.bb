// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/common/data_reduction_proxy_url_loader_throttle.h"

#include "base/bind.h"
#include "components/data_reduction_proxy/content/common/header_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_throttle_manager.h"
#include "components/data_reduction_proxy/core/common/uma_util.h"
#include "net/base/load_flags.h"

namespace net {
class HttpRequestHeaders;
}

namespace data_reduction_proxy {

DataReductionProxyURLLoaderThrottle::DataReductionProxyURLLoaderThrottle(
    const net::HttpRequestHeaders& post_cache_headers,
    DataReductionProxyThrottleManager* manager)
    : post_cache_headers_(post_cache_headers), manager_(manager) {
  DCHECK(manager);
}

DataReductionProxyURLLoaderThrottle::~DataReductionProxyURLLoaderThrottle() {}

void DataReductionProxyURLLoaderThrottle::DetachFromCurrentSequence() {}

void DataReductionProxyURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  url_chain_.clear();
  url_chain_.push_back(request->url);
  request_method_ = request->method;
  is_main_frame_ = request->resource_type == content::RESOURCE_TYPE_MAIN_FRAME;
  final_load_flags_ = request->load_flags;

  MaybeSetAcceptTransformHeader(
      request->url, static_cast<content::ResourceType>(request->resource_type),
      request->previews_state, &request->custom_proxy_pre_cache_headers);
  request->custom_proxy_post_cache_headers = post_cache_headers_;

  if (request->resource_type == content::RESOURCE_TYPE_MEDIA)
    request->custom_proxy_use_alternate_proxy_list = true;
}

void DataReductionProxyURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::ResourceResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers) {
  url_chain_.push_back(redirect_info->new_url);
  request_method_ = redirect_info->new_method;
}

void DataReductionProxyURLLoaderThrottle::BeforeWillProcessResponse(
    const GURL& response_url,
    const network::ResourceResponseHead& response_head,
    bool* defer) {
  if (response_head.was_fetched_via_cache)
    return;

  DCHECK_EQ(response_url, url_chain_.back());
  DCHECK(!pending_restart_);

  const net::ProxyServer& proxy_server = response_head.proxy_server;

  // No need to retry fetch of warmup URLs since it is useful to fetch the
  // warmup URL only via a data saver proxy.
  if (params::IsWarmupURL(response_url))
    return;

  before_will_process_response_received_ = true;
  MaybeRetry(proxy_server, response_head.headers.get(), net::OK, defer);
}

void DataReductionProxyURLLoaderThrottle::MaybeRetry(
    const net::ProxyServer& proxy_server,
    const net::HttpResponseHeaders* headers,
    net::Error net_error,
    bool* defer) {
  // The set of data reduction proxy servers to mark as bad prior to
  // restarting the request.
  std::vector<net::ProxyServer> bad_proxies;

  // TODO(https://crbug.com/721403): Implement retry due to authentication
  // failure.

  // TODO(https://crbug.com/721403): Need the actual bad proxies map. Since
  // this is only being used for some metrics logging not a big deal.
  net::ProxyRetryInfoMap proxy_retry_info;

  DataReductionProxyInfo data_reduction_proxy_info;

  DataReductionProxyBypassType bypass_type = BYPASS_EVENT_TYPE_MAX;

  DataReductionProxyBypassProtocol protocol;
  pending_restart_ = protocol.MaybeBypassProxyAndPrepareToRetry(
      request_method_, url_chain_, headers, proxy_server, net_error,
      proxy_retry_info,
      manager_->FindConfiguredDataReductionProxy(proxy_server), &bypass_type,
      &data_reduction_proxy_info, &bad_proxies, &pending_restart_load_flags_);

  if (!bad_proxies.empty())
    MarkProxiesAsBad(bad_proxies, data_reduction_proxy_info.bypass_duration);

  // TODO(https://crbug.com/721403): Log bypass stats.

  // If proxies are being marked as bad the throttle needs to defer. The
  // throttle will later be resumed  (and possibly restartd) in
  // OnMarkProxiesAsBadComplete()).
  if (waiting_for_mark_proxies_) {
    *defer = true;
  } else {
    DoPendingRestart();
  }
}

void DataReductionProxyURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::ResourceResponseHead* response_head,
    bool* defer) {
  base::Optional<DataReductionProxyTypeInfo> proxy_info =
      manager_->FindConfiguredDataReductionProxy(response_head->proxy_server);
  if (!proxy_info || (final_load_flags_ & net::LOAD_BYPASS_PROXY) != 0)
    return;

  LogSuccessfulProxyUMAs(proxy_info.value(), response_head->proxy_server,
                         is_main_frame_);
}

void DataReductionProxyURLLoaderThrottle::WillOnCompleteWithError(
    const network::URLLoaderCompletionStatus& status,
    bool* defer) {
  if (!before_will_process_response_received_) {
    MaybeRetry(status.proxy_server, nullptr,
               static_cast<net::Error>(status.error_code), defer);
  }
}

void DataReductionProxyURLLoaderThrottle::MarkProxiesAsBad(
    const std::vector<net::ProxyServer>& bad_proxies,
    base::TimeDelta bypass_duration) {
  DCHECK(!waiting_for_mark_proxies_);
  DCHECK(!bad_proxies.empty());

  // Convert |bad_proxies| to a net::ProxyList that is expected by the mojo
  // interface.
  net::ProxyList proxy_list;
  for (const auto& proxy : bad_proxies)
    proxy_list.AddProxyServer(proxy);

  auto callback = base::BindOnce(
      &DataReductionProxyURLLoaderThrottle::OnMarkProxiesAsBadComplete,
      weak_factory_.GetWeakPtr());

  waiting_for_mark_proxies_ = true;
  manager_->MarkProxiesAsBad(bypass_duration, proxy_list, std::move(callback));
}

void DataReductionProxyURLLoaderThrottle::OnMarkProxiesAsBadComplete() {
  DCHECK(waiting_for_mark_proxies_);
  waiting_for_mark_proxies_ = false;

  DoPendingRestart();

  // Un-defer the throttle.
  delegate_->Resume();
}

void DataReductionProxyURLLoaderThrottle::DoPendingRestart() {
  if (!pending_restart_)
    return;

  int load_flags = pending_restart_load_flags_;

  pending_restart_ = false;
  pending_restart_load_flags_ = 0;
  final_load_flags_ |= load_flags;

  delegate_->RestartWithFlags(load_flags);
}

}  // namespace data_reduction_proxy
