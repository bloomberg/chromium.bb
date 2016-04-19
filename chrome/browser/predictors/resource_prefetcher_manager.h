// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCHER_MANAGER_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCHER_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetcher.h"

namespace net {
class URLRequestContextGetter;
}

namespace predictors {

struct NavigationID;
class ResourcePrefetchPredictor;

// Manages prefetches for multple navigations.
//  - Created and owned by the resource prefetch predictor.
//  - Needs to be refcounted as it is de-referenced on two different threads.
//  - Created on the UI thread, but most functions are called in the IO thread.
//  - Will only allow one inflight prefresh per main frame URL.
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

  // Will create a new ResourcePrefetcher for the main frame url of the input
  // navigation if there isn't one already for the same URL or host (for host
  // based).
  void MaybeAddPrefetch(
      const NavigationID& navigation_id,
      PrefetchKeyType key_type,
      std::unique_ptr<ResourcePrefetcher::RequestVector> requests);

  // Stops the ResourcePrefetcher for the input navigation, if one was in
  // progress.
  void MaybeRemovePrefetch(const NavigationID& navigation_id);

  // ResourcePrefetcher::Delegate methods.
  void ResourcePrefetcherFinished(
      ResourcePrefetcher* prefetcher,
      ResourcePrefetcher::RequestVector* requests) override;
  net::URLRequestContext* GetURLRequestContext() override;

 private:
  friend class base::RefCountedThreadSafe<ResourcePrefetcherManager>;
  friend class MockResourcePrefetcherManager;

  typedef std::map<std::string, ResourcePrefetcher*> PrefetcherMap;

  ~ResourcePrefetcherManager() override;

  // UI Thread. |predictor_| needs to be called on the UI thread.
  void ResourcePrefetcherFinishedOnUI(
      const NavigationID& navigation_id,
      PrefetchKeyType key_type,
      std::unique_ptr<ResourcePrefetcher::RequestVector> requests);

  ResourcePrefetchPredictor* predictor_;
  const ResourcePrefetchPredictorConfig config_;
  net::URLRequestContextGetter* const context_getter_;

  PrefetcherMap prefetcher_map_;  // Owns the ResourcePrefetcher pointers.

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetcherManager);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCHER_MANAGER_H_
