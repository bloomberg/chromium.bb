// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCHER_MANAGER_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCHER_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetcher.h"

namespace net {
class URLRequestContextGetter;
}

namespace predictors {

class ResourcePrefetchPredictor;

// Manages prefetches for multiple navigations.
//  - Created and owned by the resource prefetch predictor.
//  - Needs to be refcounted as it is de-referenced on two different threads.
//  - Created on the UI thread, but most functions are called in the IO thread.
//  - Will only allow one inflight prefresh per host.
class ResourcePrefetcherManager
    :  public ResourcePrefetcher::Delegate,
       public base::RefCountedThreadSafe<ResourcePrefetcherManager> {
 public:
  // The |predictor| should be alive till ShutdownOnIOThread is called.
  ResourcePrefetcherManager(ResourcePrefetchPredictor* predictor,
                            const ResourcePrefetchPredictorConfig& config,
                            net::URLRequestContextGetter* getter);

  // UI thread.
  void ShutdownOnUIThread();

  // --- IO Thread methods.

  // The prefetchers need to be deleted on the IO thread.
  void ShutdownOnIOThread();

  // Will create a new ResourcePrefetcher for a given main frame url if there
  // isn't one already for the same host.
  void MaybeAddPrefetch(const GURL& main_frame_url,
                        const std::vector<GURL>& urls);

  // Stops the ResourcePrefetcher for a given main frame URL, if one was in
  // progress.
  void MaybeRemovePrefetch(const GURL& main_frame_url);

  // ResourcePrefetcher::Delegate methods.
  void ResourcePrefetcherFinished(
      ResourcePrefetcher* prefetcher,
      std::unique_ptr<ResourcePrefetcher::PrefetcherStats> stats) override;
  net::URLRequestContext* GetURLRequestContext() override;

 private:
  friend class base::RefCountedThreadSafe<ResourcePrefetcherManager>;
  friend class MockResourcePrefetcherManager;

  ~ResourcePrefetcherManager() override;

  ResourcePrefetchPredictor* predictor_;
  const ResourcePrefetchPredictorConfig config_;
  net::URLRequestContextGetter* const context_getter_;

  std::map<std::string, std::unique_ptr<ResourcePrefetcher>> prefetcher_map_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetcherManager);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCHER_MANAGER_H_
