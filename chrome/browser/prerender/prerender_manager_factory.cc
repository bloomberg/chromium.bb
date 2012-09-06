// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_manager_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/prerender_condition_network.h"
#endif

namespace prerender {

// static
PrerenderManager* PrerenderManagerFactory::GetForProfile(
    Profile* profile) {
  if (!PrerenderManager::IsPrerenderingPossible())
    return NULL;
  return static_cast<PrerenderManager*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
PrerenderManagerFactory* PrerenderManagerFactory::GetInstance() {
  return Singleton<PrerenderManagerFactory>::get();
}

PrerenderManagerFactory::PrerenderManagerFactory()
    : ProfileKeyedServiceFactory("PrerenderManager",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

PrerenderManagerFactory::~PrerenderManagerFactory() {
}

ProfileKeyedService* PrerenderManagerFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  CHECK(g_browser_process->prerender_tracker());
  PrerenderManager* prerender_manager = new PrerenderManager(
      profile, g_browser_process->prerender_tracker());
#if defined(OS_CHROMEOS)
  if (chromeos::CrosLibrary::Get()) {
    prerender_manager->AddCondition(
        new chromeos::PrerenderConditionNetwork(
            chromeos::CrosLibrary::Get()->GetNetworkLibrary()));
  }
#endif
  return prerender_manager;
}

bool PrerenderManagerFactory::ServiceHasOwnInstanceInIncognito() const {
  return true;
}

}  // namespace prerender
