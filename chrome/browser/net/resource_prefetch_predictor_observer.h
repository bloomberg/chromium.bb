// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_RESOURCE_PREFETCH_PREDICTOR_OBSERVER_H_
#define CHROME_BROWSER_NET_RESOURCE_PREFETCH_PREDICTOR_OBSERVER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"

namespace net {
class URLRequest;
}

class GURL;

namespace chrome_browser_net {

// Observes resource requests in the ResourceDispatcherHostDelegate and notifies
// the ResourcePrefetchPredictor about the ones it is interested in.
//  - Has an instance per profile, and is owned by the corresponding
//    ProfileIOData.
//  - Needs to be constructed on UI thread. Can be destroyed on UI or IO thread.
//    As for member functions, public members are meant to be called on the IO
//    thread and private members from the UI thread.
class ResourcePrefetchPredictorObserver {
 public:
  explicit ResourcePrefetchPredictorObserver(
      predictors::ResourcePrefetchPredictor* predictor);
  ~ResourcePrefetchPredictorObserver();

  // Parts of the ResourceDispatcherHostDelegate that we want to observe.
  void OnRequestStarted(net::URLRequest* request,
                        content::ResourceType resource_type,
                        const content::ResourceRequestInfo::WebContentsGetter&
                            web_contents_getter);
  void OnRequestRedirected(
      net::URLRequest* request,
      const GURL& redirect_url,
      const content::ResourceRequestInfo::WebContentsGetter&
          web_contents_getter);
  void OnResponseStarted(net::URLRequest* request,
                         const content::ResourceRequestInfo::WebContentsGetter&
                             web_contents_getter);

 private:
  void OnRequestStartedOnUIThread(
      std::unique_ptr<predictors::ResourcePrefetchPredictor::URLRequestSummary>
          summary,
      const content::ResourceRequestInfo::WebContentsGetter&
          web_contents_getter,
      const GURL& main_frame_url,
      const base::TimeTicks& creation_time) const;
  void OnRequestRedirectedOnUIThread(
      std::unique_ptr<predictors::ResourcePrefetchPredictor::URLRequestSummary>
          summary,
      const content::ResourceRequestInfo::WebContentsGetter&
          web_contents_getter,
      const GURL& main_frame_url,
      const base::TimeTicks& creation_time) const;
  void OnResponseStartedOnUIThread(
      std::unique_ptr<predictors::ResourcePrefetchPredictor::URLRequestSummary>
          summary,
      const content::ResourceRequestInfo::WebContentsGetter&
          web_contents_getter,
      const GURL& main_frame_url,
      const base::TimeTicks& creation_time) const;

  // Owned by profile.
  base::WeakPtr<predictors::ResourcePrefetchPredictor> predictor_;

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetchPredictorObserver);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_RESOURCE_PREFETCH_PREDICTOR_OBSERVER_H_
