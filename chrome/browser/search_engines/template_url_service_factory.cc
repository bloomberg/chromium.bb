// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_factory.h"

#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

TemplateURLService* TemplateURLServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TemplateURLService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

TemplateURLServiceFactory* TemplateURLServiceFactory::GetInstance() {
  return Singleton<TemplateURLServiceFactory>::get();
}

TemplateURLServiceFactory::TemplateURLServiceFactory()
    : ProfileKeyedServiceFactory(
        ProfileDependencyManager::GetInstance()) {
  // TODO(erg): For Shutdown() order, we need to:
  //     DependsOn(WebDataServiceFactory::GetInstance());
  //     DependsOn(HistoryService::GetInstance());
  //     DependsOn(ExtensionService::GetInstance());
}

TemplateURLServiceFactory::~TemplateURLServiceFactory() {}

ProfileKeyedService* TemplateURLServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new TemplateURLService(profile);
}

bool TemplateURLServiceFactory::ServiceRedirectedInIncognito() {
  return true;
}

void TemplateURLServiceFactory::ProfileShutdown(Profile* profile) {
  // We shutdown AND destroy the TemplateURLService during this pass.
  // TemplateURLService schedules a task on the WebDataService from its
  // destructor. Delete it first to ensure the task gets scheduled before we
  // shut down the database.
  ProfileKeyedServiceFactory::ProfileShutdown(profile);
  ProfileKeyedServiceFactory::ProfileDestroyed(profile);
}

void TemplateURLServiceFactory::ProfileDestroyed(Profile* profile) {
  // Don't double delete.
}
