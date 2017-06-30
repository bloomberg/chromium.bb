// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_TRACKER_FEATURE_ENGAGEMENT_TRACKER_FACTORY_H_
#define IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_TRACKER_FEATURE_ENGAGEMENT_TRACKER_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace feature_engagement_tracker {
class FeatureEngagementTracker;
}  // namespace feature_engagement_tracker

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// FeatureEngagementTrackerFactory is the main class for interacting with the
// feature_engagement_tracker component. It uses the KeyedService API to
// expose functions to associate and retrieve a FeatureEngagementTracker object
// with a given ios::ChromeBrowserState object.
class FeatureEngagementTrackerFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns the FeatureEngagementTrackerFactory singleton object.
  static FeatureEngagementTrackerFactory* GetInstance();

  // Retrieves the FeatureEngagementTracker object associated with a given
  // browser state. It is created if it does not already exist.
  static feature_engagement_tracker::FeatureEngagementTracker*
  GetForBrowserState(ios::ChromeBrowserState* browser_state);

 protected:
  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;

 private:
  friend struct base::DefaultSingletonTraits<FeatureEngagementTrackerFactory>;

  FeatureEngagementTrackerFactory();
  ~FeatureEngagementTrackerFactory() override;

  DISALLOW_COPY_AND_ASSIGN(FeatureEngagementTrackerFactory);
};

#endif  // IOS_CHROME_BROWSER_FEATURE_ENGAGEMENT_TRACKER_FEATURE_ENGAGEMENT_TRACKER_FACTORY_H_
