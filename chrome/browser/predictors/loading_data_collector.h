// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_DATA_COLLECTOR_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_DATA_COLLECTOR_H_

#include <string>

#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "content/public/common/resource_type.h"

namespace net {
class URLRequest;
}

namespace predictors {

// Records to the database and stats collection classes navigation events as
// reported by various observers. All the non-static methods of this class need
// to be called on the UI thread.
class LoadingDataCollector {
 public:
  explicit LoadingDataCollector(ResourcePrefetchPredictor* predictor);
  ~LoadingDataCollector();

  // Thread safe.
  static bool ShouldRecordRequest(net::URLRequest* request,
                                  content::ResourceType resource_type);
  static bool ShouldRecordResponse(net::URLRequest* response);
  static bool ShouldRecordRedirect(net::URLRequest* response);

  // 'LoadingPredictorObserver' and 'ResourcePrefetchPredictorTabHelper' call
  // the below functions to inform the predictor of main frame and resource
  // requests. Should only be called if the corresponding Should* functions
  // return true.
  void RecordURLRequest(
      const ResourcePrefetchPredictor::URLRequestSummary& request);
  void RecordURLResponse(
      const ResourcePrefetchPredictor::URLRequestSummary& response);
  void RecordURLRedirect(
      const ResourcePrefetchPredictor::URLRequestSummary& response);

  // Called when the main frame of a page completes loading.
  void RecordMainFrameLoadComplete(const NavigationID& navigation_id);

  // Called after the main frame's first contentful paint.
  void RecordFirstContentfulPaint(
      const NavigationID& navigation_id,
      const base::TimeTicks& first_contentful_paint);

 private:
  friend class ResourcePrefetchPredictorBrowserTest;

  FRIEND_TEST_ALL_PREFIXES(LoadingDataCollectorTest, HandledResourceTypes);

  // Returns true if the main page request is supported for prediction.
  static bool IsHandledMainPage(net::URLRequest* request);

  // Returns true if the subresource request is supported for prediction.
  static bool IsHandledSubresource(net::URLRequest* request,
                                   content::ResourceType resource_type);

  // Returns true if the subresource has a supported type.
  static bool IsHandledResourceType(content::ResourceType resource_type,
                                    const std::string& mime_type);

  static void SetAllowPortInUrlsForTesting(bool state);

  ResourcePrefetchPredictor* const predictor_;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_DATA_COLLECTOR_H_
