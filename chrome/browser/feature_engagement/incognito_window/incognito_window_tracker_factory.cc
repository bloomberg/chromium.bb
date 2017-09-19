// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/incognito_window/incognito_window_tracker_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/feature_engagement/incognito_window/incognito_window_tracker.h"
#include "chrome/browser/feature_engagement/session_duration_updater.h"
#include "chrome/browser/feature_engagement/session_duration_updater_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace feature_engagement {

// static
IncognitoWindowTrackerFactory* IncognitoWindowTrackerFactory::GetInstance() {
  return base::Singleton<IncognitoWindowTrackerFactory>::get();
}

IncognitoWindowTracker* IncognitoWindowTrackerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<IncognitoWindowTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

IncognitoWindowTrackerFactory::IncognitoWindowTrackerFactory()
    : BrowserContextKeyedServiceFactory(
          "IncognitoWindowTracker",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SessionDurationUpdaterFactory::GetInstance());
  DependsOn(TrackerFactory::GetInstance());
}

IncognitoWindowTrackerFactory::~IncognitoWindowTrackerFactory() = default;

KeyedService* IncognitoWindowTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new IncognitoWindowTracker(
      Profile::FromBrowserContext(context),
      feature_engagement::SessionDurationUpdaterFactory::GetInstance()
          ->GetForProfile(Profile::FromBrowserContext(context)));
}

content::BrowserContext* IncognitoWindowTrackerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace feature_engagement
