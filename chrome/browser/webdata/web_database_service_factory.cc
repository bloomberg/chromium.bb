// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_database_service_factory.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/webdata/web_database_service_impl.h"
#include "chrome/common/chrome_constants.h"

WebDatabaseServiceFactory::WebDatabaseServiceFactory()
    : ProfileKeyedServiceFactory(
          "WebDatabaseService",
          ProfileDependencyManager::GetInstance()) {
  // WebDatabaseServiceFactory has no dependecies, however WebDataService
  // depends on us.
}

WebDatabaseServiceFactory::~WebDatabaseServiceFactory() {}

// static
WebDatabaseService* WebDatabaseService::FromBrowserContext(
    content::BrowserContext* context) {
  // For this service, the implicit/explicit distinction doesn't
  // really matter; it's just used for a DCHECK.  So we currently
  // cheat and always say EXPLICIT_ACCESS.
  return WebDatabaseServiceFactory::GetForProfile(
      static_cast<Profile*>(context), Profile::EXPLICIT_ACCESS);
}

// static
WebDatabaseService* WebDatabaseServiceFactory::GetForProfile(
    Profile* profile, Profile::ServiceAccessType access_type) {
  // If |access_type| starts being used for anything other than this
  // DCHECK, we need to start taking it as a parameter to
  // AutofillWebDataServiceImpl::FromBrowserContext (see above).
  DCHECK(access_type != Profile::IMPLICIT_ACCESS || !profile->IsOffTheRecord());
  return static_cast<WebDatabaseServiceImpl*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
WebDatabaseService* WebDatabaseServiceFactory::GetForProfileIfExists(
    Profile* profile, Profile::ServiceAccessType access_type) {
  DCHECK(access_type != Profile::IMPLICIT_ACCESS || !profile->IsOffTheRecord());
  return static_cast<WebDatabaseServiceImpl*>(
      GetInstance()->GetServiceForProfile(profile, false));
}

// static
WebDatabaseServiceFactory* WebDatabaseServiceFactory::GetInstance() {
  return Singleton<WebDatabaseServiceFactory>::get();
}

bool WebDatabaseServiceFactory::ServiceRedirectedInIncognito() const {
  return true;
}

ProfileKeyedService*
    WebDatabaseServiceFactory::BuildServiceInstanceFor(Profile* profile) const {
  DCHECK(profile);
  base::FilePath path = profile->GetPath().Append(chrome::kWebDataFilename);
  scoped_ptr<WebDatabaseServiceImpl> wdbs(new WebDatabaseServiceImpl(path));
  wdbs->LoadDatabase(WebDatabaseService::InitCallback());
  return wdbs.release();
}

bool WebDatabaseServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
