// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_service_factory.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/webdata/autofill_web_data_service_impl.h"
#include "chrome/common/chrome_constants.h"

WebDataServiceWrapper::WebDataServiceWrapper() {}

WebDataServiceWrapper::WebDataServiceWrapper(Profile* profile) {
  base::FilePath path = profile->GetPath();
  path = path.Append(chrome::kWebDataFilename);
  web_data_service_ = new WebDataService();
  web_data_service_->Init(path);
}

WebDataServiceWrapper::~WebDataServiceWrapper() {
}

void WebDataServiceWrapper::Shutdown() {
  web_data_service_->ShutdownOnUIThread();
  web_data_service_ = NULL;
}

scoped_refptr<WebDataService> WebDataServiceWrapper::GetWebData() {
  return web_data_service_.get();
}

// static
scoped_refptr<AutofillWebDataService>
AutofillWebDataService::FromBrowserContext(content::BrowserContext* context) {
  // For this service, the implicit/explicit distinction doesn't
  // really matter; it's just used for a DCHECK.  So we currently
  // cheat and always say EXPLICIT_ACCESS.
  scoped_refptr<WebDataService> service = WebDataServiceFactory::GetForProfile(
      static_cast<Profile*>(context), Profile::EXPLICIT_ACCESS);

  if (service.get()) {
    return scoped_refptr<AutofillWebDataService>(
        new AutofillWebDataServiceImpl(service));
  } else {
    return scoped_refptr<AutofillWebDataService>(NULL);
  }
}

// static
scoped_refptr<WebDataService> WebDataService::FromBrowserContext(
    content::BrowserContext* context) {
  // For this service, the implicit/explicit distinction doesn't
  // really matter; it's just used for a DCHECK.  So we currently
  // cheat and always say EXPLICIT_ACCESS.
  return WebDataServiceFactory::GetForProfile(
      static_cast<Profile*>(context), Profile::EXPLICIT_ACCESS);
}

WebDataServiceFactory::WebDataServiceFactory()
    : ProfileKeyedServiceFactory(
          "WebDataService",
          ProfileDependencyManager::GetInstance()) {
  // WebDataServiceFactory has no dependecies.
}

WebDataServiceFactory::~WebDataServiceFactory() {}

// static
scoped_refptr<WebDataService> WebDataServiceFactory::GetForProfile(
    Profile* profile, Profile::ServiceAccessType access_type) {
  // If |access_type| starts being used for anything other than this
  // DCHECK, we need to start taking it as a parameter to
  // AutofillWebDataServiceImpl::FromBrowserContext (see above).
  DCHECK(access_type != Profile::IMPLICIT_ACCESS || !profile->IsOffTheRecord());
  WebDataServiceWrapper* wrapper =
      static_cast<WebDataServiceWrapper*>(
          GetInstance()->GetServiceForProfile(profile, true));
  if (wrapper)
    return wrapper->GetWebData();
  // |wrapper| can be NULL in Incognito mode.
  return NULL;
}

// static
scoped_refptr<WebDataService> WebDataServiceFactory::GetForProfileIfExists(
    Profile* profile, Profile::ServiceAccessType access_type) {
  // If |access_type| starts being used for anything other than this
  // DCHECK, we need to start taking it as a parameter to
  // AutofillWebDataServiceImpl::FromBrowserContext (see above).
  DCHECK(access_type != Profile::IMPLICIT_ACCESS || !profile->IsOffTheRecord());
  WebDataServiceWrapper* wrapper =
      static_cast<WebDataServiceWrapper*>(
          GetInstance()->GetServiceForProfile(profile, true));
  if (wrapper)
    return wrapper->GetWebData();
  // |wrapper| can be NULL in Incognito mode.
  return NULL;
}

// static
WebDataServiceFactory* WebDataServiceFactory::GetInstance() {
  return Singleton<WebDataServiceFactory>::get();
}

bool WebDataServiceFactory::ServiceRedirectedInIncognito() const {
  return true;
}

ProfileKeyedService*
WebDataServiceFactory::BuildServiceInstanceFor(Profile* profile) const {
  return new WebDataServiceWrapper(profile);
}

bool WebDataServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
