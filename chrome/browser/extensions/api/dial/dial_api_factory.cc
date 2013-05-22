// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/dial/dial_api_factory.h"

#include "chrome/browser/extensions/api/dial/dial_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace extensions {

// static
scoped_refptr<DialAPI> DialAPIFactory::GetForProfile(Profile* profile) {
  return static_cast<DialAPI*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get());
}

// static
DialAPIFactory* DialAPIFactory::GetInstance() {
  return Singleton<DialAPIFactory>::get();
}

DialAPIFactory::DialAPIFactory() : RefcountedBrowserContextKeyedServiceFactory(
    "DialAPI", BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

DialAPIFactory::~DialAPIFactory() {
}

scoped_refptr<RefcountedBrowserContextKeyedService>
    DialAPIFactory::BuildServiceInstanceFor(
        content::BrowserContext* profile) const {
  return scoped_refptr<DialAPI>(new DialAPI(static_cast<Profile*>(profile)));
}

bool DialAPIFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool DialAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
