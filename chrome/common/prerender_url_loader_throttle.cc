// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/prerender_url_loader_throttle.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/common/prerender_util.h"
#include "content/public/common/content_constants.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace prerender {

namespace {

const char kPurposeHeaderName[] = "Purpose";
const char kPurposeHeaderValue[] = "prefetch";

void CallCancelPrerenderForUnsupportedScheme(
    mojo::PendingRemote<chrome::mojom::PrerenderCanceler> canceler,
    const GURL& url) {
  mojo::Remote<chrome::mojom::PrerenderCanceler>(std::move(canceler))
      ->CancelPrerenderForUnsupportedScheme(url);
}

// Returns true if the response has a "no-store" cache control header.
bool IsNoStoreResponse(const network::mojom::URLResponseHead& response_head) {
  return response_head.headers &&
         response_head.headers->HasHeaderValue("cache-control", "no-store");
}

}  // namespace

PrerenderURLLoaderThrottle::PrerenderURLLoaderThrottle(
    PrerenderMode mode,
    const std::string& histogram_prefix,
    mojo::PendingRemote<chrome::mojom::PrerenderCanceler> canceler)
    : mode_(mode),
      histogram_prefix_(histogram_prefix),
      canceler_(std::move(canceler)) {
  DCHECK(canceler_);
}

PrerenderURLLoaderThrottle::~PrerenderURLLoaderThrottle() {
  if (destruction_closure_)
    std::move(destruction_closure_).Run();
}

void PrerenderURLLoaderThrottle::PrerenderUsed() {
  if (original_request_priority_)
    delegate_->SetPriority(original_request_priority_.value());
  if (deferred_)
    delegate_->Resume();
}

void PrerenderURLLoaderThrottle::DetachFromCurrentSequence() {
  // This method is only called for synchronous XHR from the main thread which
  // should not occur during a NoStatePrerender.
  NOTREACHED();
}

void PrerenderURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  if (mode_ == PREFETCH_ONLY) {
    request->load_flags |= net::LOAD_PREFETCH;
    request->cors_exempt_headers.SetHeader(kPurposeHeaderName,
                                           kPurposeHeaderValue);
  }

  resource_type_ =
      static_cast<blink::mojom::ResourceType>(request->resource_type);
  // Abort any prerenders that spawn requests that use unsupported HTTP
  // methods or schemes.
  if (!IsValidHttpMethod(mode_, request->method)) {
    // If this is a full prerender, cancel the prerender in response to
    // invalid requests.  For prefetches, cancel invalid requests but keep the
    // prefetch going.
    delegate_->CancelWithError(net::ERR_ABORTED);
  }

  if (request->resource_type !=
          static_cast<int>(blink::mojom::ResourceType::kMainFrame) &&
      !DoesSubresourceURLHaveValidScheme(request->url)) {
    // Destroying the prerender for unsupported scheme only for non-main
    // resource to allow chrome://crash to actually crash in the
    // *RendererCrash tests instead of being intercepted here. The
    // unsupported scheme for the main resource is checked in
    // WillRedirectRequest() and PrerenderContents::CheckURL(). See
    // http://crbug.com/673771.
    delegate_->CancelWithError(net::ERR_ABORTED);
    CallCancelPrerenderForUnsupportedScheme(std::move(canceler_), request->url);
    return;
  }

#if defined(OS_ANDROID)
  if (request->resource_type ==
      static_cast<int>(blink::mojom::ResourceType::kFavicon)) {
    // Delay icon fetching until the contents are getting swapped in
    // to conserve network usage in mobile devices.
    *defer = true;
    return;
  }
#else
  // Priorities for prerendering requests are lowered, to avoid competing with
  // other page loads, except on Android where this is less likely to be a
  // problem. In some cases, this may negatively impact the performance of
  // prerendering, see https://crbug.com/652746 for details.
  // Requests with the IGNORE_LIMITS flag set (i.e., sync XHRs)
  // should remain at MAXIMUM_PRIORITY.
  if (request->load_flags & net::LOAD_IGNORE_LIMITS) {
    DCHECK_EQ(request->priority, net::MAXIMUM_PRIORITY);
  } else if (request->priority != net::IDLE) {
    original_request_priority_ = request->priority;
    request->priority = net::IDLE;
  }
#endif  // OS_ANDROID

  if (mode_ == PREFETCH_ONLY) {
    detached_timer_.Start(FROM_HERE,
                          base::TimeDelta::FromMilliseconds(
                              content::kDefaultDetachableCancelDelayMs),
                          this, &PrerenderURLLoaderThrottle::OnTimedOut);
  }
}

void PrerenderURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* /* to_be_removed_headers */,
    net::HttpRequestHeaders* /* modified_headers */,
    net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
  redirect_count_++;
  if (mode_ == PREFETCH_ONLY) {
    RecordPrefetchResponseReceived(
        histogram_prefix_, blink::IsResourceTypeFrame(resource_type_),
        true /* is_redirect */, IsNoStoreResponse(response_head));
  }

  std::string follow_only_when_prerender_shown_header;
  if (response_head.headers) {
    response_head.headers->GetNormalizedHeader(
        kFollowOnlyWhenPrerenderShown,
        &follow_only_when_prerender_shown_header);
  }
  // Abort any prerenders with requests which redirect to invalid schemes.
  if (!DoesURLHaveValidScheme(redirect_info->new_url)) {
    delegate_->CancelWithError(net::ERR_ABORTED);
    CallCancelPrerenderForUnsupportedScheme(std::move(canceler_),
                                            redirect_info->new_url);
  } else if (follow_only_when_prerender_shown_header == "1" &&
             resource_type_ != blink::mojom::ResourceType::kMainFrame) {
    // Only defer redirects with the Follow-Only-When-Prerender-Shown
    // header. Do not defer redirects on main frame loads.
    *defer = true;
    deferred_ = true;
  }
}

void PrerenderURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  if (mode_ != PREFETCH_ONLY)
    return;

  bool is_main_resource = blink::IsResourceTypeFrame(resource_type_);
  RecordPrefetchResponseReceived(histogram_prefix_, is_main_resource,
                                 true /* is_redirect */,
                                 IsNoStoreResponse(*response_head));
  RecordPrefetchRedirectCount(histogram_prefix_, is_main_resource,
                              redirect_count_);
}

void PrerenderURLLoaderThrottle::OnTimedOut() {
  delegate_->CancelWithError(net::ERR_ABORTED);
}

}  // namespace prerender
