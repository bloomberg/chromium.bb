// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/history/top_sites_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
scoped_refptr<history::TopSites> TopSitesFactory::GetForProfile(
    Profile* profile) {
  return static_cast<history::TopSites*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get());
}

// static
scoped_refptr<history::TopSites> TopSitesFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<history::TopSites*>(
      GetInstance()->GetServiceForBrowserContext(profile, false).get());
}

// static
TopSitesFactory* TopSitesFactory::GetInstance() {
  return Singleton<TopSitesFactory>::get();
}

TopSitesFactory::TopSitesFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "TopSites",
          BrowserContextDependencyManager::GetInstance()) {
}

TopSitesFactory::~TopSitesFactory() {
}

scoped_refptr<RefcountedKeyedService> TopSitesFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  history::TopSitesImpl* top_sites =
      new history::TopSitesImpl(static_cast<Profile*>(context));
  top_sites->Init(context->GetPath().Append(chrome::kTopSitesFilename));
  return make_scoped_refptr(top_sites);
}

bool TopSitesFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
