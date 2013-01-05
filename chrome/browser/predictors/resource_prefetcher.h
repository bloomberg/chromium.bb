// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCHER_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCHER_H_

#include <map>
#include <list>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

namespace net {
class URLRequestContext;
}

namespace predictors {

// Responsible for prefetching resources for a single navigation based on the
// input list of resources.
//  - Limits the max number of resources in flight for any host and also across
//    hosts.
//  - When stopped, will wait for the pending requests to finish.
//  - Lives entirely on the IO thread.
class ResourcePrefetcher : public base::NonThreadSafe,
                           public net::URLRequest::Delegate {
 public:
  // Denotes the prefetch request for a single subresource.
  struct Request {
    explicit Request(const GURL& i_resource_url);
    Request(const Request& other);

    enum PrefetchStatus {
      PREFETCH_STATUS_NOT_STARTED,
      PREFETCH_STATUS_STARTED,

      // Cancellation reasons.
      PREFETCH_STATUS_REDIRECTED,
      PREFETCH_STATUS_AUTH_REQUIRED,
      PREFETCH_STATUS_CERT_REQUIRED,
      PREFETCH_STATUS_CERT_ERROR,
      PREFETCH_STATUS_CANCELLED,
      PREFETCH_STATUS_FAILED,

      // Successful prefetch states.
      PREFETCH_STATUS_FROM_CACHE,
      PREFETCH_STATUS_FROM_NETWORK
    };

    enum UsageStatus {
      USAGE_STATUS_NOT_REQUESTED,
      USAGE_STATUS_FROM_CACHE,
      USAGE_STATUS_FROM_NETWORK,
      USAGE_STATUS_NAVIGATION_ABANDONED
    };

    GURL resource_url;
    PrefetchStatus prefetch_status;
    UsageStatus usage_status;
  };
  typedef ScopedVector<Request> RequestVector;

  // Used to communicate when the prefetching is done. All methods are invoked
  // on the IO thread.
  class Delegate {
   public:
    virtual ~Delegate() { }

    // Called when the ResourcePrefetcher is finished, i.e. there is nothing
    // pending in flight. Should take ownership of |requests|.
    virtual void ResourcePrefetcherFinished(
        ResourcePrefetcher* prefetcher,
        RequestVector* requests) = 0;

    virtual net::URLRequestContext* GetURLRequestContext() = 0;
  };

  // |delegate| has to outlive the ResourcePrefetcher. The ResourcePrefetcher
  // takes ownership of |requests|.
  ResourcePrefetcher(Delegate* delegate,
                     const ResourcePrefetchPredictorConfig& config,
                     const NavigationID& navigation_id,
                     PrefetchKeyType key_type,
                     scoped_ptr<RequestVector> requests);
  virtual ~ResourcePrefetcher();

  void Start();  // Kicks off the prefetching. Can only be called once.
  void Stop();   // No additional prefetches will be queued after this.

  const NavigationID& navigation_id() const { return navigation_id_; }
  PrefetchKeyType key_type() const { return key_type_; }

 private:
  friend class ResourcePrefetcherTest;
  friend class TestResourcePrefetcher;

  // Launches new prefetch requests if possible.
  void TryToLaunchPrefetchRequests();

  // Starts a net::URLRequest for the input |request|.
  void SendRequest(Request* request);

  // Called by |SendRequest| to start the |request|. This is necessary to stub
  // out the Start() call to net::URLRequest for unittesting.
  virtual void StartURLRequest(net::URLRequest* request);

  // Marks the request as finished, with the given status.
  void FinishRequest(net::URLRequest* request, Request::PrefetchStatus status);

  // Reads the response data from the response - required for the resource to
  // be cached correctly. Stubbed out during testing.
  virtual void ReadFullResponse(net::URLRequest* request);

  // Returns true if the request has more data that needs to be read. If it
  // returns false, the request should not be referenced again.
  bool ShouldContinueReadingRequest(net::URLRequest* request, int bytes_read);

  // net::URLRequest::Delegate methods.
  virtual void OnReceivedRedirect(net::URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect) OVERRIDE;
  virtual void OnAuthRequired(net::URLRequest* request,
                              net::AuthChallengeInfo* auth_info) OVERRIDE;
  virtual void OnCertificateRequested(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) OVERRIDE;
  virtual void OnSSLCertificateError(net::URLRequest* request,
                                     const net::SSLInfo& ssl_info,
                                     bool fatal) OVERRIDE;
  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE;
  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE;

  enum PrefetcherState {
    INITIALIZED = 0,  // Prefetching hasn't started.
    RUNNING = 1,      // Prefetching started, allowed to add more requests.
    STOPPED = 2,      // Prefetching started, not allowed to add more requests.
    FINISHED = 3      // No more inflight request, new requests not possible.
  };

  PrefetcherState state_;
  Delegate* const delegate_;
  ResourcePrefetchPredictorConfig const config_;
  NavigationID navigation_id_;
  PrefetchKeyType key_type_;
  scoped_ptr<RequestVector> request_vector_;

  std::map<net::URLRequest*, Request*> inflight_requests_;
  std::list<Request*> request_queue_;
  std::map<std::string, int> host_inflight_counts_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetcher);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCHER_H_
