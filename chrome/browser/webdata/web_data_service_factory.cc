// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_service_factory.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/ui/profile_error_dialog.h"
#include "chrome/browser/webdata/autocomplete_syncable_service.h"
#include "chrome/browser/webdata/autofill_profile_syncable_service.h"
#include "chrome/browser/webdata/autofill_web_data_service_impl.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_thread.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

using content::BrowserThread;

namespace {

// Callback to show error dialog on profile load error.
void ProfileErrorCallback(sql::InitStatus status) {
  ShowProfileErrorDialog(
      (status == sql::INIT_FAILURE) ?
      IDS_COULDNT_OPEN_PROFILE_ERROR : IDS_PROFILE_TOO_NEW_ERROR);
}

void InitSyncableServicesOnDBThread(scoped_refptr<WebDataService> web_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  // Currently only Autocomplete and Autofill profiles use the new Sync API, but
  // all the database data should migrate to this API over time.
  AutocompleteSyncableService::CreateForWebDataService(web_data);
  AutofillProfileSyncableService::CreateForWebDataService(web_data);
}

}  // namespace

WebDataServiceWrapper::WebDataServiceWrapper() {}

WebDataServiceWrapper::WebDataServiceWrapper(Profile* profile) {
  base::FilePath path = profile->GetPath();
  path = path.Append(chrome::kWebDataFilename);
  web_data_service_ = new WebDataService(base::Bind(&ProfileErrorCallback));
  web_data_service_->Init(path);

  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
                          base::Bind(&InitSyncableServicesOnDBThread,
                                     web_data_service_));
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
