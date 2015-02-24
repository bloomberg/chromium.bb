// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_debug_resource_throttle.h"

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_debug_ui_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

namespace data_reduction_proxy {

namespace {
const char kResourceThrottleLogName[] =
    "DataReductionProxyDebugResourceThrottle";
}  // namespace

// static
scoped_ptr<DataReductionProxyDebugResourceThrottle>
DataReductionProxyDebugResourceThrottle::MaybeCreate(
    const net::URLRequest* request,
    content::ResourceType resource_type,
    const DataReductionProxyIOData* io_data) {
  if (io_data && io_data->IsEnabled() &&
      data_reduction_proxy::DataReductionProxyParams::
          WarnIfNoDataReductionProxy()) {
    DCHECK(io_data->debug_ui_service());
    DCHECK(request);
    return scoped_ptr<DataReductionProxyDebugResourceThrottle>(
        new DataReductionProxyDebugResourceThrottle(
            request, resource_type, io_data->debug_ui_service(),
            io_data->config()));
  }
  return nullptr;
}

// static
void DataReductionProxyDebugResourceThrottle::StartDisplayingBlockingPage(
    scoped_refptr<DataReductionProxyDebugUIManager> ui_manager,
    const DataReductionProxyDebugUIManager::BypassResource& resource) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ui_manager->DisplayBlockingPage(resource);
}

DataReductionProxyDebugResourceThrottle::
DataReductionProxyDebugResourceThrottle(
    const net::URLRequest* request,
    content::ResourceType resource_type,
    const DataReductionProxyDebugUIService* ui_service,
    const DataReductionProxyConfig* config)
    : state_(NOT_BYPASSED),
      request_(request),
      ui_service_(ui_service),
      config_(config),
      is_subresource_(resource_type != content::RESOURCE_TYPE_MAIN_FRAME) {
  DCHECK(request);
  DCHECK(ui_service);
  DCHECK(config);
}

DataReductionProxyDebugResourceThrottle::
~DataReductionProxyDebugResourceThrottle() {
}

// Displays an interstitial when the proxy will not be used for a request
// because it isn't configured.
void DataReductionProxyDebugResourceThrottle::WillStartUsingNetwork(
    bool* defer) {
  DCHECK_EQ(NOT_BYPASSED, state_);
  if (!config_->AreDataReductionProxiesBypassed(
          *request_, ui_service_->data_reduction_proxy_config(), NULL)) {
    return;
  }
  if (request_->load_flags() & net::LOAD_PREFETCH) {
    // Don't prefetch resources that bypass, disallow them.
    controller()->Cancel();
    return;
  }
  // Do not display the interstitial if bypassed by local rules.
  if (config_->IsBypassedByDataReductionProxyLocalRules(
          *request_, ui_service_->data_reduction_proxy_config())) {
    state_ = LOCAL_BYPASS;
    return;
  }
  DisplayBlockingPage(defer);
}

// Displays an intersitital when the Data Reduction Proxy explicitly returns a
// response that triggers a bypass.
void DataReductionProxyDebugResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info, bool* defer) {
  // If the interstitial has already been shown, do not show it again. The
  // LOAD_BYPASS_PROXY flag makes it so that proxies are not resolved. This
  // override is used when downloading PAC files. If the request does not have
  // the LOAD_BYPASS_PROXY flag, do not show the interstitial.
  if (state_ != NOT_BYPASSED ||
      !(request_->load_flags() & net::LOAD_BYPASS_PROXY)) {
    return;
  }
  DisplayBlockingPage(defer);
}

const char* DataReductionProxyDebugResourceThrottle::GetNameForLogging() const {
  return kResourceThrottleLogName;
}

void DataReductionProxyDebugResourceThrottle::DisplayBlockingPage(bool* defer) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);
  DCHECK(info);

  state_ = REMOTE_BYPASS;
  *defer = true;

  DataReductionProxyDebugUIManager::BypassResource resource;
  resource.url = request_->url();
  resource.is_subresource = is_subresource_;
  resource.callback = base::Bind(
      &DataReductionProxyDebugResourceThrottle::OnBlockingPageComplete,
      AsWeakPtr());
  resource.render_process_host_id = info->GetChildID();
  resource.render_view_id = info->GetRouteID();

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(
          &DataReductionProxyDebugResourceThrottle::StartDisplayingBlockingPage,
          ui_service_->ui_manager(), resource));
}

void
DataReductionProxyDebugResourceThrottle::OnBlockingPageComplete(bool proceed) {
  state_ = NOT_BYPASSED;
  if (proceed)
    controller()->Resume();
  else
    controller()->Cancel();
}

}  // namespace data_reduction_proxy
