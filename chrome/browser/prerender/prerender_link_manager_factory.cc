// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_link_manager_factory.h"

#include "chrome/browser/prerender/prerender_link_manager.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace prerender {

// static
PrerenderLinkManager* PrerenderLinkManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PrerenderLinkManager*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
PrerenderLinkManagerFactory* PrerenderLinkManagerFactory::GetInstance() {
  return Singleton<PrerenderLinkManagerFactory>::get();
}

PrerenderLinkManagerFactory::PrerenderLinkManagerFactory()
    : ProfileKeyedServiceFactory("PrerenderLinkmanager",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(prerender::PrerenderManagerFactory::GetInstance());
}

ProfileKeyedService* PrerenderLinkManagerFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForProfile(profile);
  if (!prerender_manager)
    return NULL;
  PrerenderLinkManager* prerender_link_manager =
      new PrerenderLinkManager(prerender_manager);
  return prerender_link_manager;
}

bool PrerenderLinkManagerFactory::ServiceHasOwnInstanceInIncognito() const {
  return true;
}

}  // namespace prerender
