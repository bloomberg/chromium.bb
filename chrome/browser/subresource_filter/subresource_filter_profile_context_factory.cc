// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"

// static
SubresourceFilterProfileContext*
SubresourceFilterProfileContextFactory::GetForProfile(Profile* profile) {
  return static_cast<SubresourceFilterProfileContext*>(
      GetInstance()->GetServiceForBrowserContext(profile, true /* create */));
}

// static
SubresourceFilterProfileContextFactory*
SubresourceFilterProfileContextFactory::GetInstance() {
  return base::Singleton<SubresourceFilterProfileContextFactory>::get();
}

SubresourceFilterProfileContextFactory::SubresourceFilterProfileContextFactory()
    : BrowserContextKeyedServiceFactory(
          "SubresourceFilterProfileContext",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* SubresourceFilterProfileContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new SubresourceFilterProfileContext(static_cast<Profile*>(profile));
}

content::BrowserContext*
SubresourceFilterProfileContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
