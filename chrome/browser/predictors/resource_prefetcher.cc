// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iterator>

#include "chrome/browser/predictors/resource_prefetcher.h"

#include "base/stl_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"

namespace {

// The size of the buffer used to read the resource.
static const size_t kResourceBufferSizeBytes = 50000;

}  // namespace

namespace predictors {

ResourcePrefetcher::Request::Request(const GURL& i_resource_url)
    : resource_url(i_resource_url),
      prefetch_status(PREFETCH_STATUS_NOT_STARTED),
      usage_status(USAGE_STATUS_NOT_REQUESTED) {
}

ResourcePrefetcher::Request::Request(const Request& other)
    : resource_url(other.resource_url),
      prefetch_status(other.prefetch_status),
      usage_status(other.usage_status) {
}

ResourcePrefetcher::ResourcePrefetcher(
    Delegate* delegate,
    const ResourcePrefetchPredictorConfig& config,
    const NavigationID& navigation_id,
    PrefetchKeyType key_type,
    scoped_ptr<RequestVector> requests)
        : state_(INITIALIZED),
          delegate_(delegate),
          config_(config),
          navigation_id_(navigation_id),
          key_type_(key_type),
          request_vector_(requests.Pass()) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(request_vector_.get());

  std::copy(request_vector_->begin(), request_vector_->end(),
            std::back_inserter(request_queue_));
}

ResourcePrefetcher::~ResourcePrefetcher() {
  // Delete any pending net::URLRequests.
  STLDeleteContainerPairFirstPointers(inflight_requests_.begin(),
                                      inflight_requests_.end());
}

void ResourcePrefetcher::Start() {
  CHECK(CalledOnValidThread());

  CHECK_EQ(state_, INITIALIZED);
  state_ = RUNNING;

  TryToLaunchPrefetchRequests();
}

void ResourcePrefetcher::Stop() {
  CHECK(CalledOnValidThread());

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
    while ((static_cast<int>(inflight_requests_.size()) <
                config_.max_prefetches_inflight_per_navigation) &&
           request_available) {
      std::list<Request*>::iterator request_it = request_queue_.begin();
      for (; request_it != request_queue_.end(); ++request_it) {
        const std::string& host = (*request_it)->resource_url.host();

        std::map<std::string, int>::iterator host_it =
            host_inflight_counts_.find(host);
        if (host_it == host_inflight_counts_.end() ||
            host_it->second <
                config_.max_prefetches_inflight_per_host_per_navigation)
          break;
      }
      request_available = request_it != request_queue_.end();

      if (request_available) {
        SendRequest(*request_it);
        request_queue_.erase(request_it);
      }
    }
  }

  // If the inflight_requests_ is empty, we cant launch any more. Finish.
  if (inflight_requests_.empty()) {
    CHECK(host_inflight_counts_.empty());
    CHECK(request_queue_.empty() || state_ == STOPPED);

    state_ = FINISHED;
    delegate_->ResourcePrefetcherFinished(this, request_vector_.release());
  }
}

void ResourcePrefetcher::SendRequest(Request* request) {
  request->prefetch_status = Request::PREFETCH_STATUS_STARTED;

  net::URLRequest* url_request =
      new net::URLRequest(request->resource_url,
                          this,
                          delegate_->GetURLRequestContext());
  inflight_requests_[url_request] = request;
  host_inflight_counts_[url_request->original_url().host()] += 1;

  url_request->set_method("GET");
  url_request->set_first_party_for_cookies(navigation_id_.main_frame_url);
  url_request->set_referrer(navigation_id_.main_frame_url.spec());
  url_request->set_priority(net::LOW);
  StartURLRequest(url_request);
}

void ResourcePrefetcher::StartURLRequest(net::URLRequest* request) {
  request->Start();
}

void ResourcePrefetcher::FinishRequest(net::URLRequest* request,
                                       Request::PrefetchStatus status) {
  std::map<net::URLRequest*, Request*>::iterator request_it =
      inflight_requests_.find(request);
  CHECK(request_it != inflight_requests_.end());

  const std::string host = request->original_url().host();
  std::map<std::string, int>::iterator host_it = host_inflight_counts_.find(
      host);
  CHECK_GT(host_it->second, 0);
  host_it->second -= 1;
  if (host_it->second == 0)
    host_inflight_counts_.erase(host);

  request_it->second->prefetch_status = status;
  inflight_requests_.erase(request_it);

  delete request;

  TryToLaunchPrefetchRequests();
}

void ResourcePrefetcher::ReadFullResponse(net::URLRequest* request) {
  bool status = true;
  while (status) {
    int bytes_read = 0;
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(
        kResourceBufferSizeBytes));
    status = request->Read(buffer, kResourceBufferSizeBytes, &bytes_read);

    if (status) {
      status = ShouldContinueReadingRequest(request, bytes_read);
    } else if (request->status().error()) {
      FinishRequest(request, Request::PREFETCH_STATUS_FAILED);
      return;
    }
  }
}

bool ResourcePrefetcher::ShouldContinueReadingRequest(net::URLRequest* request,
                                                      int bytes_read) {
  if (bytes_read == 0) {  // When bytes_read == 0, no more data.
    if (request->was_cached())
      FinishRequest(request, Request::PREFETCH_STATUS_FROM_CACHE);
    else
      FinishRequest(request, Request::PREFETCH_STATUS_FROM_NETWORK);
    return false;
  }

  return true;
}

void ResourcePrefetcher::OnReceivedRedirect(net::URLRequest* request,
                                            const GURL& new_url,
                                            bool* defer_redirect) {
  FinishRequest(request, Request::PREFETCH_STATUS_REDIRECTED);
}

void ResourcePrefetcher::OnAuthRequired(net::URLRequest* request,
                                        net::AuthChallengeInfo* auth_info) {
  FinishRequest(request, Request::PREFETCH_STATUS_AUTH_REQUIRED);
}

void ResourcePrefetcher::OnCertificateRequested(
    net::URLRequest* request,
    net::SSLCertRequestInfo* cert_request_info) {
  FinishRequest(request, Request::PREFETCH_STATUS_CERT_REQUIRED);
}

void ResourcePrefetcher::OnSSLCertificateError(net::URLRequest* request,
                                               const net::SSLInfo& ssl_info,
                                               bool fatal) {
  FinishRequest(request, Request::PREFETCH_STATUS_CERT_ERROR);
}

void ResourcePrefetcher::OnResponseStarted(net::URLRequest* request) {
  if (request->status().error()) {
    FinishRequest(request, Request::PREFETCH_STATUS_FAILED);
    return;
  }

  // TODO(shishir): Do not read cached entries, or ones that are not cacheable.
  ReadFullResponse(request);
}

void ResourcePrefetcher::OnReadCompleted(net::URLRequest* request,
                                         int bytes_read) {
  if (request->status().error()) {
    FinishRequest(request, Request::PREFETCH_STATUS_FAILED);
    return;
  }

  if (ShouldContinueReadingRequest(request, bytes_read))
    ReadFullResponse(request);
}

}  // namespace predictors
