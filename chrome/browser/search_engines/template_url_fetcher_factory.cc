// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_fetcher_factory.h"

#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/search_engines/template_url_fetcher.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"

// static
TemplateURLFetcher* TemplateURLFetcherFactory::GetForProfile(
    Profile* profile) {
  return static_cast<TemplateURLFetcher*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
TemplateURLFetcherFactory* TemplateURLFetcherFactory::GetInstance() {
  return Singleton<TemplateURLFetcherFactory>::get();
}

// static
void TemplateURLFetcherFactory::ShutdownForProfile(Profile* profile) {
  TemplateURLFetcherFactory* factory = GetInstance();
  factory->ProfileShutdown(profile);
  factory->ProfileDestroyed(profile);
}

TemplateURLFetcherFactory::TemplateURLFetcherFactory()
    : ProfileKeyedServiceFactory("TemplateURLFetcher",
                                 ProfileDependencyManager::GetInstance()) {
    DependsOn(TemplateURLServiceFactory::GetInstance());
}

TemplateURLFetcherFactory::~TemplateURLFetcherFactory() {
}

ProfileKeyedService* TemplateURLFetcherFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new TemplateURLFetcher(profile);
}

bool TemplateURLFetcherFactory::ServiceRedirectedInIncognito() const {
  return true;
}
