// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"

// static
void SubresourceFilterContentSettingsManagerFactory::EnsureForProfile(
    Profile* profile) {
  GetInstance()->GetServiceForBrowserContext(profile, true /* create */);
}

// static
SubresourceFilterContentSettingsManagerFactory*
SubresourceFilterContentSettingsManagerFactory::GetInstance() {
  return base::Singleton<SubresourceFilterContentSettingsManagerFactory>::get();
}

SubresourceFilterContentSettingsManagerFactory::
    SubresourceFilterContentSettingsManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "SubresourceFilterContentSettingsManager",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService*
SubresourceFilterContentSettingsManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new SubresourceFilterContentSettingsManager(
      static_cast<Profile*>(profile));
}
