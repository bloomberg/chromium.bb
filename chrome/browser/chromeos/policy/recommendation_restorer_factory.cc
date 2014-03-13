// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/recommendation_restorer_factory.h"

#include "chrome/browser/chromeos/policy/recommendation_restorer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace policy {

// static
RecommendationRestorerFactory* RecommendationRestorerFactory::GetInstance() {
  return Singleton<RecommendationRestorerFactory>::get();
}

// static
RecommendationRestorer* RecommendationRestorerFactory::GetForProfile(
    Profile* profile) {
  return reinterpret_cast<RecommendationRestorer*>(
          GetInstance()->GetServiceForBrowserContext(profile, false));
}

KeyedService* RecommendationRestorerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new RecommendationRestorer(static_cast<Profile*>(context));
}

bool RecommendationRestorerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

RecommendationRestorerFactory::RecommendationRestorerFactory()
    : BrowserContextKeyedServiceFactory(
          "RecommendationRestorer",
          BrowserContextDependencyManager::GetInstance()) {
}

RecommendationRestorerFactory::~RecommendationRestorerFactory() {
}

}  // namespace policy
