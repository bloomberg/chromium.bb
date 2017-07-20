// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement_tracker/new_tab/new_tab_tracker_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/feature_engagement_tracker/feature_engagement_tracker_factory.h"
#include "chrome/browser/feature_engagement_tracker/new_tab/new_tab_tracker.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace feature_engagement_tracker {

// static
NewTabTrackerFactory* NewTabTrackerFactory::GetInstance() {
  return base::Singleton<NewTabTrackerFactory>::get();
}

NewTabTracker* NewTabTrackerFactory::GetForProfile(Profile* profile) {
  return static_cast<NewTabTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

NewTabTrackerFactory::NewTabTrackerFactory()
    : BrowserContextKeyedServiceFactory(
          "NewTabTracker",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(FeatureEngagementTrackerFactory::GetInstance());
}

NewTabTrackerFactory::~NewTabTrackerFactory() = default;

KeyedService* NewTabTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new NewTabTracker(Profile::FromBrowserContext(context));
}

content::BrowserContext* NewTabTrackerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace feature_engagement_tracker
