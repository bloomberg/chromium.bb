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
#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/autofill_web_data_service_impl.h"
#include "chrome/browser/webdata/keyword_table.h"
#include "chrome/browser/webdata/logins_table.h"
#include "chrome/browser/webdata/token_service_table.h"
#include "chrome/browser/webdata/web_apps_table.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_intents_table.h"
#include "chrome/browser/webdata/webdata_constants.h"
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

void InitSyncableServicesOnDBThread(
    scoped_refptr<AutofillWebDataService> autofill_web_data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  // Currently only Autocomplete and Autofill profiles use the new Sync API, but
  // all the database data should migrate to this API over time.
  AutocompleteSyncableService::CreateForWebDataService(autofill_web_data);
  AutofillProfileSyncableService::CreateForWebDataService(autofill_web_data);
}

}  // namespace

WebDataServiceWrapper::WebDataServiceWrapper() {}

WebDataServiceWrapper::WebDataServiceWrapper(Profile* profile) {
  base::FilePath path = profile->GetPath();
  path = path.Append(kWebDataFilename);

  web_database_ = new WebDatabaseService(path);

  // All tables objects that participate in managing the database must
  // be added here.
  web_database_->AddTable(
      scoped_ptr<WebDatabaseTable>(new AutofillTable()));
  web_database_->AddTable(
      scoped_ptr<WebDatabaseTable>(new KeywordTable()));
  // TODO(mdm): We only really need the LoginsTable on Windows for IE7 password
  // access, but for now, we still create it on all platforms since it deletes
  // the old logins table. We can remove this after a while, e.g. in M22 or so.
  web_database_->AddTable(
      scoped_ptr<WebDatabaseTable>(new LoginsTable()));
  web_database_->AddTable(
      scoped_ptr<WebDatabaseTable>(new TokenServiceTable()));
  web_database_->AddTable(
      scoped_ptr<WebDatabaseTable>(new WebAppsTable()));
  // TODO(thakis): Add a migration to delete the SQL table used by
  // WebIntentsTable, then remove this.
  web_database_->AddTable(
      scoped_ptr<WebDatabaseTable>(new WebIntentsTable()));

  // TODO (caitkp): Rework the callbacks here. They're ugly.

  web_database_->LoadDatabase(WebDatabaseService::InitCallback());

  autofill_web_data_ = new AutofillWebDataServiceImpl(
      web_database_, base::Bind(&ProfileErrorCallback));
  autofill_web_data_->Init();

  web_data_ = new WebDataService(
      web_database_, base::Bind(&ProfileErrorCallback));
  web_data_->Init();

  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
                          base::Bind(&InitSyncableServicesOnDBThread,
                                     autofill_web_data_));
}

WebDataServiceWrapper::~WebDataServiceWrapper() {
}

void WebDataServiceWrapper::Shutdown() {
  autofill_web_data_->ShutdownOnUIThread();
  web_data_->ShutdownOnUIThread();
  web_database_->ShutdownDatabase();
}

scoped_refptr<AutofillWebDataService>
WebDataServiceWrapper::GetAutofillWebData() {
  return autofill_web_data_.get();
}

scoped_refptr<WebDataService> WebDataServiceWrapper::GetWebData() {
  return web_data_.get();
}

// static
scoped_refptr<AutofillWebDataService>
AutofillWebDataService::FromBrowserContext(content::BrowserContext* context) {
  // For this service, the implicit/explicit distinction doesn't
  // really matter; it's just used for a DCHECK.  So we currently
  // cheat and always say EXPLICIT_ACCESS.
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(
          static_cast<Profile*>(context), Profile::EXPLICIT_ACCESS);
  if (wrapper)
    return wrapper->GetAutofillWebData();
  // |wrapper| can be NULL in Incognito mode.
  return scoped_refptr<AutofillWebDataService>(NULL);
}

// static
scoped_refptr<WebDataService> WebDataService::FromBrowserContext(
    content::BrowserContext* context) {
  // For this service, the implicit/explicit distinction doesn't
  // really matter; it's just used for a DCHECK.  So we currently
  // cheat and always say EXPLICIT_ACCESS.
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(
          static_cast<Profile*>(context), Profile::EXPLICIT_ACCESS);
  if (wrapper)
    return wrapper->GetWebData();
  // |wrapper| can be NULL in Incognito mode.
  return scoped_refptr<WebDataService>(NULL);
}

WebDataServiceFactory::WebDataServiceFactory()
    : ProfileKeyedServiceFactory(
          "WebDataService",
          ProfileDependencyManager::GetInstance()) {
  // WebDataServiceFactory has no dependecies.
}

WebDataServiceFactory::~WebDataServiceFactory() {}

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForProfile(
    Profile* profile, Profile::ServiceAccessType access_type) {
  // If |access_type| starts being used for anything other than this
  // DCHECK, we need to start taking it as a parameter to
  // AutofillWebDataServiceImpl::FromBrowserContext (see above).
  DCHECK(access_type != Profile::IMPLICIT_ACCESS || !profile->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
          GetInstance()->GetServiceForProfile(profile, true));
}

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForProfileIfExists(
    Profile* profile, Profile::ServiceAccessType access_type) {
  // If |access_type| starts being used for anything other than this
  // DCHECK, we need to start taking it as a parameter to
  // AutofillWebDataServiceImpl::FromBrowserContext (see above).
  DCHECK(access_type != Profile::IMPLICIT_ACCESS || !profile->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
          GetInstance()->GetServiceForProfile(profile, false));
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
