// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_navigation/web_navigation_api_factory.h"

#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
WebNavigationAPIFactory* WebNavigationAPIFactory::GetInstance() {
  return Singleton<WebNavigationAPIFactory>::get();
}

WebNavigationAPIFactory::WebNavigationAPIFactory()
    : ProfileKeyedServiceFactory("WebNavigationAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

WebNavigationAPIFactory::~WebNavigationAPIFactory() {
}

ProfileKeyedService* WebNavigationAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new WebNavigationAPI(profile);
}

bool WebNavigationAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool WebNavigationAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
