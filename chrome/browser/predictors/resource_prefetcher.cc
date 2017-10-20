// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetcher.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/referrer.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_context.h"
#include "url/origin.h"

namespace {

// The size of the buffer used to read the resource.
static const size_t kResourceBufferSizeBytes = 50000;

}  // namespace

namespace predictors {

ResourcePrefetcher::PrefetchedRequestStats::PrefetchedRequestStats(
    const GURL& resource_url,
    bool was_cached,
    size_t total_received_bytes)
    : resource_url(resource_url),
      was_cached(was_cached),
      total_received_bytes(total_received_bytes) {}

ResourcePrefetcher::PrefetchedRequestStats::~PrefetchedRequestStats() {}

ResourcePrefetcher::PrefetcherStats::PrefetcherStats(const GURL& url)
    : url(url) {}

ResourcePrefetcher::PrefetcherStats::~PrefetcherStats() {}

ResourcePrefetcher::PrefetcherStats::PrefetcherStats(
    const PrefetcherStats& other)
    : url(other.url),
      start_time(other.start_time),
      requests_stats(other.requests_stats) {}

ResourcePrefetcher::ResourcePrefetcher(
    base::WeakPtr<Delegate> delegate,
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    size_t max_concurrent_requests,
    size_t max_concurrent_requests_per_host,
    const GURL& main_frame_url,
    const std::vector<GURL>& urls)
    : state_(INITIALIZED),
      delegate_(std::move(delegate)),
      context_getter_(std::move(context_getter)),
      max_concurrent_requests_(max_concurrent_requests),
      max_concurrent_requests_per_host_(max_concurrent_requests_per_host),
      main_frame_url_(main_frame_url),
      prefetched_count_(0),
      prefetched_bytes_(0),
      request_queue_(urls.begin(), urls.end()),
      stats_(base::MakeUnique<PrefetcherStats>(main_frame_url)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ResourcePrefetcher::~ResourcePrefetcher() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void ResourcePrefetcher::Start() {
  TRACE_EVENT_ASYNC_BEGIN0("browser", "ResourcePrefetcher::Prefetch", this);
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  CHECK_EQ(state_, INITIALIZED);
  state_ = RUNNING;

  stats_->start_time = base::TimeTicks::Now();
  TryToLaunchPrefetchRequests();
}

void ResourcePrefetcher::Stop() {
  TRACE_EVENT_ASYNC_END0("browser", "ResourcePrefetcher::Prefetch", this);
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (state_ == FINISHED)
    return;

  state_ = STOPPED;
}

void ResourcePrefetcher::TryToLaunchPrefetchRequests() {
  CHECK(state_ == RUNNING || state_ == STOPPED);

  // Try to launch new requests if the state is RUNNING.
  if (state_ == RUNNING) {
    bool request_available = true;

    // Loop through the requests while we are under the
    // max_prefetches_inflight_per_host_per_navigation limit, looking for a URL
    // for which the max_prefetches_inflight_per_host_per_navigation limit has
    // not been reached. Try to launch as many requests as possible.
    while ((inflight_requests_.size() < max_concurrent_requests_) &&
           request_available) {
      auto request_it = request_queue_.begin();
      for (; request_it != request_queue_.end(); ++request_it) {
        const std::string& host = request_it->host();

        std::map<std::string, size_t>::iterator host_it =
            host_inflight_counts_.find(host);
        if (host_it == host_inflight_counts_.end() ||
            host_it->second < max_concurrent_requests_per_host_)
          break;
      }
      request_available = request_it != request_queue_.end();

      if (request_available) {
        SendRequest(*request_it);
        request_queue_.erase(request_it);
      }
    }
  }

  // If the inflight_requests_ is empty, we can't launch any more. Finish.
  if (inflight_requests_.empty()) {
    CHECK(host_inflight_counts_.empty());
    CHECK(request_queue_.empty() || state_ == STOPPED);

    UMA_HISTOGRAM_COUNTS_100(
        internal::kResourcePrefetchPredictorPrefetchedCountHistogram,
        prefetched_count_);
    UMA_HISTOGRAM_COUNTS_10000(
        internal::kResourcePrefetchPredictorPrefetchedSizeHistogram,
        prefetched_bytes_ / 1024);

    state_ = FINISHED;

    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::BindOnce(&Delegate::ResourcePrefetcherFinished, delegate_, this,
                       base::Passed(std::move(stats_))));
  }
}

void ResourcePrefetcher::SendRequest(const GURL& url) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("resource_prefetch", R"(
        semantics {
          sender: "Resource Prefetch"
          description:
            "Speculative Prefetch is based on the observation that users do "
            "most of their browsing to a limited number of websites (per "
            "device). It observes subresource requests made during a page "
            "load, and builds a local database of likely resources, keyed by "
            "URL and/or hostname. When a user navigates (or intends to navigate"
            ") to a URL, this database is leveraged to pre-emptively fetch "
            "subresources."
          trigger:
            "Prefetching can start from two different origins:\n"
            " - When a navigation hint is given (for instance, from Custom "
            "Tabs), before any actual navigation.\n"
            " - At navigation time.\n"
            "Given a URL of the page that a user is going to navigate to, "
            "ResourcePrefetchPredictor generates a list of resources (from the "
            "URL or host database, after following the most likely redirect "
            "chain from the local redirect database). This list is ranked, and "
            "given to a ResourcePrefetcher."
          data:
            "An HTTP Get request to the resource."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "Users can control this feature via the 'Use prediction service to "
            "load pages more quickly' setting under Privacy. The feature is "
            "enabled by default."
          chrome_policy {
            NetworkPredictionOptions {
              policy_options {mode: MANDATORY}
              NetworkPredictionOptions: 2
            }
          }
        })");
  std::unique_ptr<net::URLRequest> url_request =
      context_getter_->GetURLRequestContext()->CreateRequest(
          url, net::IDLE, this, traffic_annotation);
  host_inflight_counts_[url.host()] += 1;

  url_request->set_method("GET");
  url_request->set_site_for_cookies(main_frame_url_);
  url_request->set_initiator(url::Origin::Create(main_frame_url_));

  content::Referrer referrer(main_frame_url_, blink::kWebReferrerPolicyDefault);
  content::Referrer sanitized_referrer =
      content::Referrer::SanitizeForRequest(url, referrer);
  content::Referrer::SetReferrerForRequest(url_request.get(),
                                           sanitized_referrer);

  url_request->SetLoadFlags(url_request->load_flags() | net::LOAD_PREFETCH);
  StartURLRequest(url_request.get());
  inflight_requests_.insert(
      std::make_pair(url_request.get(), std::move(url_request)));
}

void ResourcePrefetcher::StartURLRequest(net::URLRequest* request) {
  request->Start();
}

void ResourcePrefetcher::FinishRequest(net::URLRequest* request) {
  auto request_it = inflight_requests_.find(request);
  CHECK(request_it != inflight_requests_.end());

  const std::string host = request->original_url().host();
  std::map<std::string, size_t>::iterator host_it = host_inflight_counts_.find(
      host);
  CHECK_GT(host_it->second, 0U);
  host_it->second -= 1;
  if (host_it->second == 0)
    host_inflight_counts_.erase(host);

  inflight_requests_.erase(request_it);

  TryToLaunchPrefetchRequests();
}

void ResourcePrefetcher::ReadFullResponse(net::URLRequest* request) {
  int bytes_read = 0;
  do {
    auto buffer = base::MakeRefCounted<net::IOBuffer>(kResourceBufferSizeBytes);
    bytes_read = request->Read(buffer.get(), kResourceBufferSizeBytes);
    if (bytes_read == net::ERR_IO_PENDING) {
      return;
    } else if (bytes_read <= 0) {
      if (bytes_read == 0)
        RequestComplete(request);
      FinishRequest(request);
      return;
    }
  } while (bytes_read > 0);
}

void ResourcePrefetcher::RequestComplete(net::URLRequest* request) {
  ++prefetched_count_;
  int64_t total_received_bytes = request->GetTotalReceivedBytes();
  prefetched_bytes_ += total_received_bytes;

  UMA_HISTOGRAM_ENUMERATION(
      internal::kResourcePrefetchPredictorCachePatternHistogram,
      request->response_info().cache_entry_status,
      net::HttpResponseInfo::CacheEntryStatus::ENTRY_MAX);

  stats_->requests_stats.emplace_back(request->url(), request->was_cached(),
                                      total_received_bytes);
}

void ResourcePrefetcher::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  FinishRequest(request);
}

void ResourcePrefetcher::OnAuthRequired(net::URLRequest* request,
                                        net::AuthChallengeInfo* auth_info) {
  FinishRequest(request);
}

void ResourcePrefetcher::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  FinishRequest(request);
}

void ResourcePrefetcher::OnSSLCertificateError(net::URLRequest* request,
                                               const net::SSLInfo& ssl_info,
                                               bool fatal) {
  FinishRequest(request);
}

void ResourcePrefetcher::OnResponseStarted(net::URLRequest* request,
                                           int net_error) {
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  if (net_error != net::OK) {
    FinishRequest(request);
    return;
  }

  ReadFullResponse(request);
}

void ResourcePrefetcher::OnReadCompleted(net::URLRequest* request,
                                         int bytes_read) {
  DCHECK_NE(net::ERR_IO_PENDING, bytes_read);

  if (bytes_read <= 0) {
    FinishRequest(request);
    return;
  }

  if (bytes_read > 0)
    ReadFullResponse(request);
}

}  // namespace predictors
