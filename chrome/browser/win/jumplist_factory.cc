// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/jumplist_factory.h"

#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
scoped_refptr<JumpList> JumpListFactory::GetForProfile(Profile* profile) {
  return static_cast<JumpList*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get());
}

// static
JumpListFactory* JumpListFactory::GetInstance() {
  return base::Singleton<JumpListFactory>::get();
}

JumpListFactory::JumpListFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "JumpList",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(TabRestoreServiceFactory::GetInstance());
  DependsOn(TopSitesFactory::GetInstance());
  DependsOn(FaviconServiceFactory::GetInstance());
}

JumpListFactory::~JumpListFactory() = default;

scoped_refptr<RefcountedKeyedService> JumpListFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new JumpList(Profile::FromBrowserContext(context));
}
