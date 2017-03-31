// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_ENGAGEMENT_TRACKER_FEATURE_ENGAGEMENT_TRACKER_FACTORY_H_
#define CHROME_BROWSER_FEATURE_ENGAGEMENT_TRACKER_FEATURE_ENGAGEMENT_TRACKER_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace feature_engagement_tracker {
class FeatureEngagementTracker;
}  // namespace feature_engagement_tracker

// FeatureEngagementTrackerFactory is the main client class for interaction with
// the feature_engagement_tracker component.
class FeatureEngagementTrackerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns singleton instance of FeatureEngagementTrackerFactory.
  static FeatureEngagementTrackerFactory* GetInstance();

  // Returns the FeatureEngagementTracker associated with |context|.
  static feature_engagement_tracker::FeatureEngagementTracker*
  GetForBrowserContext(content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<FeatureEngagementTrackerFactory>;

  FeatureEngagementTrackerFactory();
  ~FeatureEngagementTrackerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(FeatureEngagementTrackerFactory);
};

#endif  // CHROME_BROWSER_FEATURE_ENGAGEMENT_TRACKER_FEATURE_ENGAGEMENT_TRACKER_FACTORY_H_
