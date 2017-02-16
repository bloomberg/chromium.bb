// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCHER_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCHER_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace net {
class URLRequestContext;
}

namespace predictors {

namespace internal {
constexpr char kResourcePrefetchPredictorCachePatternHistogram[] =
    "ResourcePrefetchPredictor.CachePattern";
constexpr char kResourcePrefetchPredictorPrefetchedCountHistogram[] =
    "ResourcePrefetchPredictor.PrefetchedCount";
constexpr char kResourcePrefetchPredictorPrefetchedSizeHistogram[] =
    "ResourcePrefetchPredictor.PrefetchedSizeKB";
}  // namespace internal

// Responsible for prefetching resources for a single main frame URL based on
// the input list of resources.
//  - Limits the max number of resources in flight for any host and also across
//    hosts.
//  - When stopped, will wait for the pending requests to finish.
//  - Lives entirely on the IO thread.
class ResourcePrefetcher : public net::URLRequest::Delegate {
 public:
  struct PrefetchedRequestStats {
    PrefetchedRequestStats(const GURL& resource_url,
                           bool was_cached,
                           size_t total_received_bytes);
    ~PrefetchedRequestStats();

    GURL resource_url;
    bool was_cached;
    size_t total_received_bytes;
  };

  struct PrefetcherStats {
    explicit PrefetcherStats(const GURL& url);
    ~PrefetcherStats();
    PrefetcherStats(const PrefetcherStats& other);

    GURL url;
    base::TimeTicks start_time;
    std::vector<PrefetchedRequestStats> requests_stats;
  };

  // Used to communicate when the prefetching is done. All methods are invoked
  // on the IO thread.
  class Delegate {
   public:
    virtual ~Delegate() { }

    // Called when the ResourcePrefetcher is finished, i.e. there is nothing
    // pending in flight.
    virtual void ResourcePrefetcherFinished(
        ResourcePrefetcher* prefetcher,
        std::unique_ptr<PrefetcherStats> stats) = 0;

    virtual net::URLRequestContext* GetURLRequestContext() = 0;
  };

  // |delegate| has to outlive the ResourcePrefetcher.
  ResourcePrefetcher(Delegate* delegate,
                     const ResourcePrefetchPredictorConfig& config,
                     const GURL& main_frame_url,
                     const std::vector<GURL>& urls);
  ~ResourcePrefetcher() override;

  void Start();  // Kicks off the prefetching. Can only be called once.
  void Stop();   // No additional prefetches will be queued after this.

  const GURL& main_frame_url() const { return main_frame_url_; }

 private:
  friend class ResourcePrefetcherTest;
  friend class TestResourcePrefetcher;

  // Launches new prefetch requests if possible.
  void TryToLaunchPrefetchRequests();

  // Starts a net::URLRequest for the input |url|.
  void SendRequest(const GURL& url);

  // Called by |SendRequest| to start the |request|. This is necessary to stub
  // out the Start() call to net::URLRequest for unittesting.
  virtual void StartURLRequest(net::URLRequest* request);

  // Marks the request as finished, with the given status.
  void FinishRequest(net::URLRequest* request);

  // Reads the response data from the response - required for the resource to
  // be cached correctly. Stubbed out during testing.
  virtual void ReadFullResponse(net::URLRequest* request);

  // Called after successfull reading of response to save stats for histograms.
  void RequestComplete(net::URLRequest* request);

  // net::URLRequest::Delegate methods.
  void OnReceivedRedirect(net::URLRequest* request,
                          const net::RedirectInfo& redirect_info,
                          bool* defer_redirect) override;
  void OnAuthRequired(net::URLRequest* request,
                      net::AuthChallengeInfo* auth_info) override;
  void OnCertificateRequested(
      net::URLRequest* request,
      net::SSLCertRequestInfo* cert_request_info) override;
  void OnSSLCertificateError(net::URLRequest* request,
                             const net::SSLInfo& ssl_info,
                             bool fatal) override;
  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  enum PrefetcherState {
    INITIALIZED = 0,  // Prefetching hasn't started.
    RUNNING = 1,      // Prefetching started, allowed to add more requests.
    STOPPED = 2,      // Prefetching started, not allowed to add more requests.
    FINISHED = 3      // No more inflight request, new requests not possible.
  };

  base::ThreadChecker thread_checker_;
  PrefetcherState state_;
  Delegate* const delegate_;
  ResourcePrefetchPredictorConfig const config_;
  GURL main_frame_url_;

  // For histogram reports.
  size_t prefetched_count_;
  int64_t prefetched_bytes_;

  std::map<net::URLRequest*, std::unique_ptr<net::URLRequest>>
      inflight_requests_;
  std::list<GURL> request_queue_;
  std::map<std::string, size_t> host_inflight_counts_;
  std::unique_ptr<PrefetcherStats> stats_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetcher);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCHER_H_
