// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_service_factory.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
SiteEngagementService* SiteEngagementServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SiteEngagementService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SiteEngagementServiceFactory* SiteEngagementServiceFactory::GetInstance() {
  return base::Singleton<SiteEngagementServiceFactory>::get();
}

SiteEngagementServiceFactory::SiteEngagementServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SiteEngagementService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

SiteEngagementServiceFactory::~SiteEngagementServiceFactory() {
}

bool SiteEngagementServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* SiteEngagementServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new SiteEngagementService(static_cast<Profile*>(profile));
}
