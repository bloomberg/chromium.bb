// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/customtabs/detached_resource_request.h"

#include <utility>

#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/referrer.h"
#include "content/public/common/resource_type.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_job.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace customtabs {

namespace {

constexpr int kMaxResponseSize = 100 * 1024;

}  // namespace

// static
void DetachedResourceRequest::CreateAndStart(
    content::BrowserContext* browser_context,
    const GURL& url,
    const GURL& site_for_cookies,
    const net::URLRequest::ReferrerPolicy referrer_policy,
    DetachedResourceRequest::OnResultCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<DetachedResourceRequest> detached_request(
      new DetachedResourceRequest(url, site_for_cookies, referrer_policy,
                                  std::move(cb)));
  Start(std::move(detached_request), browser_context);
}

DetachedResourceRequest::~DetachedResourceRequest() = default;

DetachedResourceRequest::DetachedResourceRequest(
    const GURL& url,
    const GURL& site_for_cookies,
    net::URLRequest::ReferrerPolicy referrer_policy,
    DetachedResourceRequest::OnResultCallback cb)
    : url_(url),
      site_for_cookies_(site_for_cookies),
      cb_(std::move(cb)),
      redirects_(0) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("customtabs_parallel_request",
                                          R"(
            semantics {
              sender: "Custom Tabs"
              description:
                "When a URL is opened in Custom Tabs on Android, the calling "
                "app can specify a single parallel request to be made while "
                "the main URL is loading. This allows the calling app to "
                "remove a redirect that would otherwise be needed, improving "
                "performance."
              trigger: "A page is loaded in a Custom Tabs."
              data: "Same as a regular resource request."
              destination: WEBSITE
            }
            policy {
              cookies_allowed: YES
              cookie_store: "user"
              policy_exception_justification: "Identical to a resource fetch."
            })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";
  resource_request->url = url_;
  // The referrer is stripped if it's not set properly initially.
  resource_request->referrer = net::URLRequestJob::ComputeReferrerForPolicy(
      referrer_policy, site_for_cookies_, url_);
  resource_request->referrer_policy = referrer_policy;
  resource_request->site_for_cookies = site_for_cookies_;
  resource_request->request_initiator = url::Origin::Create(site_for_cookies_);
  resource_request->resource_type = content::RESOURCE_TYPE_SUB_RESOURCE;
  resource_request->do_not_prompt_for_login = true;
  resource_request->render_frame_id = -1;
  resource_request->enable_load_timing = false;
  resource_request->report_raw_headers = false;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
}

// static
void DetachedResourceRequest::Start(
    std::unique_ptr<DetachedResourceRequest> request,
    content::BrowserContext* browser_context) {
  request->start_time_ = base::TimeTicks::Now();
  auto* storage_partition =
      content::BrowserContext::GetStoragePartition(browser_context, nullptr);

  request->url_loader_->SetOnRedirectCallback(
      base::BindRepeating(&DetachedResourceRequest::OnRedirectCallback,
                          base::Unretained(request.get())));

  // |url_loader_| is owned by the request, and must be kept alive to not cancel
  // the request. Pass the ownership of the request to the response callback,
  // ensuring that it stays alive, yet is freed upon completion or failure.
  //
  // This is also the reason for this function to be a static member function
  // instead of a regular function.
  request->url_loader_->DownloadToString(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&DetachedResourceRequest::OnResponseCallback,
                     std::move(request)),
      kMaxResponseSize);
}

void DetachedResourceRequest::OnRedirectCallback(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head) {
  redirects_++;
}

void DetachedResourceRequest::OnResponseCallback(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool success = url_loader_->NetError() == net::OK;
  auto duration = base::TimeTicks::Now() - start_time_;
  if (success) {
    // Max 20 redirects, 21 would be a bug.
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "CustomTabs.DetachedResourceRequest.RedirectsCount.Success", redirects_,
        1, 21, 21);
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "CustomTabs.DetachedResourceRequest.Duration.Success", duration);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "CustomTabs.DetachedResourceRequest.RedirectsCount.Failure", redirects_,
        1, 21, 21);
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "CustomTabs.DetachedResourceRequest.Duration.Failure", duration);
  }

  std::move(cb_).Run(success);
}

}  // namespace customtabs
