// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_service_factory.h"

#include "base/file_path.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_constants.h"

WebDataServiceFactory::WebDataServiceFactory()
    : RefcountedProfileKeyedServiceFactory(
          "WebDataService",
          ProfileDependencyManager::GetInstance()) {
  // WebDataServiceFactory has no dependecies.
}

WebDataServiceFactory::~WebDataServiceFactory() {}

// static
scoped_refptr<WebDataService> WebDataServiceFactory::GetForProfile(
    Profile* profile, Profile::ServiceAccessType access_type) {
  DCHECK(access_type != Profile::IMPLICIT_ACCESS || !profile->IsOffTheRecord());
  return static_cast<WebDataService*>(
      GetInstance()->GetServiceForProfile(profile, true).get());
}

// static
scoped_refptr<WebDataService> WebDataServiceFactory::GetForProfileIfExists(
    Profile* profile, Profile::ServiceAccessType access_type) {
  DCHECK(access_type != Profile::IMPLICIT_ACCESS || !profile->IsOffTheRecord());
  return static_cast<WebDataService*>(
      GetInstance()->GetServiceForProfile(profile, false).get());
}

// static
WebDataServiceFactory* WebDataServiceFactory::GetInstance() {
  return Singleton<WebDataServiceFactory>::get();
}

bool WebDataServiceFactory::ServiceRedirectedInIncognito() const {
  return true;
}

scoped_refptr<RefcountedProfileKeyedService>
WebDataServiceFactory::BuildServiceInstanceFor(Profile* profile) const {
  DCHECK(profile);

  FilePath path = profile->GetPath();
  path = path.Append(chrome::kWebDataFilename);

  scoped_refptr<WebDataService> wds(new WebDataService());
  if (!wds->Init(profile->GetPath()))
    NOTREACHED();
  return wds.get();
}

bool WebDataServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
