// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TAB_HELPER_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace predictors {

class ResourcePrefetchPredictorTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ResourcePrefetchPredictorTabHelper> {
 public:
  virtual ~ResourcePrefetchPredictorTabHelper();

  // content::WebContentsObserver implementation
  virtual void DocumentOnLoadCompletedInMainFrame() OVERRIDE;
  virtual void DidLoadResourceFromMemoryCache(
      const content::LoadFromMemoryCacheDetails& details) OVERRIDE;

 private:
  explicit ResourcePrefetchPredictorTabHelper(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<ResourcePrefetchPredictorTabHelper>;

  DISALLOW_COPY_AND_ASSIGN(ResourcePrefetchPredictorTabHelper);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TAB_HELPER_H_
