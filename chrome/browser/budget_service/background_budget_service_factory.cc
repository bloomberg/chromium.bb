// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/budget_service/background_budget_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/time/default_clock.h"
#include "chrome/browser/budget_service/background_budget_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

// static
BackgroundBudgetService* BackgroundBudgetServiceFactory::GetForProfile(
    content::BrowserContext* profile) {
  // Budget tracking is not supported in incognito mode.
  if (profile->IsOffTheRecord())
    return nullptr;

  return static_cast<BackgroundBudgetService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BackgroundBudgetServiceFactory* BackgroundBudgetServiceFactory::GetInstance() {
  return base::Singleton<BackgroundBudgetServiceFactory>::get();
}

BackgroundBudgetServiceFactory::BackgroundBudgetServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "BackgroundBudgetService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SiteEngagementServiceFactory::GetInstance());
}

KeyedService* BackgroundBudgetServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new BackgroundBudgetService(static_cast<Profile*>(profile));
}
