// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_engagement/session_duration_updater_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/feature_engagement/session_duration_updater.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace feature_engagement {

// static
SessionDurationUpdaterFactory* SessionDurationUpdaterFactory::GetInstance() {
  return base::Singleton<SessionDurationUpdaterFactory>::get();
}

SessionDurationUpdater* SessionDurationUpdaterFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SessionDurationUpdater*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

SessionDurationUpdaterFactory::SessionDurationUpdaterFactory()
    : BrowserContextKeyedServiceFactory(
          "SessionDurationUpdater",
          BrowserContextDependencyManager::GetInstance()) {}

SessionDurationUpdaterFactory::~SessionDurationUpdaterFactory() = default;

KeyedService* SessionDurationUpdaterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SessionDurationUpdater(
      Profile::FromBrowserContext(context)->GetPrefs());
}

content::BrowserContext* SessionDurationUpdaterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace feature_engagement
