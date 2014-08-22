// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_service_factory.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/ui/profile_error_dialog.h"
#include "chrome/browser/webdata/autocomplete_syncable_service.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/webdata/autofill_profile_syncable_service.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/webdata/logins_table.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/signin/core/browser/webdata/token_service_table.h"
#include "components/signin/core/browser/webdata/token_web_data.h"
#include "components/webdata/common/webdata_constants.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "components/password_manager/core/browser/webdata/password_web_data_service_win.h"
#endif

using autofill::AutofillWebDataService;
using autofill::AutofillProfileSyncableService;
using content::BrowserThread;

namespace {

// Callback to show error dialog on profile load error.
void ProfileErrorCallback(ProfileErrorType type, sql::InitStatus status) {
  ShowProfileErrorDialog(
      type,
      (status == sql::INIT_FAILURE) ?
          IDS_COULDNT_OPEN_PROFILE_ERROR : IDS_PROFILE_TOO_NEW_ERROR);
}

void InitSyncableServicesOnDBThread(
    scoped_refptr<AutofillWebDataService> autofill_web_data,
    const base::FilePath& profile_path,
    const std::string& app_locale,
    autofill::AutofillWebDataBackend* autofill_backend) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  // Currently only Autocomplete and Autofill profiles use the new Sync API, but
  // all the database data should migrate to this API over time.
  AutocompleteSyncableService::CreateForWebDataServiceAndBackend(
      autofill_web_data.get(), autofill_backend);
  AutocompleteSyncableService::FromWebDataService(autofill_web_data.get())
      ->InjectStartSyncFlare(
          sync_start_util::GetFlareForSyncableService(profile_path));
  AutofillProfileSyncableService::CreateForWebDataServiceAndBackend(
      autofill_web_data.get(), autofill_backend, app_locale);
  AutofillProfileSyncableService::FromWebDataService(autofill_web_data.get())
      ->InjectStartSyncFlare(
          sync_start_util::GetFlareForSyncableService(profile_path));
}

}  // namespace

WebDataServiceWrapper::WebDataServiceWrapper() {
}

WebDataServiceWrapper::WebDataServiceWrapper(Profile* profile) {
  base::FilePath profile_path = profile->GetPath();
  base::FilePath path = profile_path.Append(kWebDataFilename);

  scoped_refptr<base::MessageLoopProxy> ui_thread =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  scoped_refptr<base::MessageLoopProxy> db_thread =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB);
  web_database_ = new WebDatabaseService(path, ui_thread, db_thread);

  // All tables objects that participate in managing the database must
  // be added here.
  web_database_->AddTable(scoped_ptr<WebDatabaseTable>(
      new autofill::AutofillTable(g_browser_process->GetApplicationLocale())));
  web_database_->AddTable(scoped_ptr<WebDatabaseTable>(new KeywordTable()));
  // TODO(mdm): We only really need the LoginsTable on Windows for IE7 password
  // access, but for now, we still create it on all platforms since it deletes
  // the old logins table. We can remove this after a while, e.g. in M22 or so.
  web_database_->AddTable(scoped_ptr<WebDatabaseTable>(new LoginsTable()));
  web_database_->AddTable(
      scoped_ptr<WebDatabaseTable>(new TokenServiceTable()));

  web_database_->LoadDatabase();

  autofill_web_data_ = new AutofillWebDataService(
      web_database_,
      ui_thread,
      db_thread,
      base::Bind(&ProfileErrorCallback, PROFILE_ERROR_DB_AUTOFILL_WEB_DATA));
  autofill_web_data_->Init();

  keyword_web_data_ = new KeywordWebDataService(
      web_database_,
      ui_thread,
      base::Bind(&ProfileErrorCallback, PROFILE_ERROR_DB_KEYWORD_WEB_DATA));
  keyword_web_data_->Init();

  token_web_data_ = new TokenWebData(
      web_database_,
      ui_thread,
      db_thread,
      base::Bind(&ProfileErrorCallback, PROFILE_ERROR_DB_TOKEN_WEB_DATA));
  token_web_data_->Init();

#if defined(OS_WIN)
  password_web_data_ = new PasswordWebDataService(
      web_database_,
      ui_thread,
      base::Bind(&ProfileErrorCallback, PROFILE_ERROR_DB_WEB_DATA));
  password_web_data_->Init();
#endif

  autofill_web_data_->GetAutofillBackend(
      base::Bind(&InitSyncableServicesOnDBThread,
                 autofill_web_data_,
                 profile_path,
                 g_browser_process->GetApplicationLocale()));
}

WebDataServiceWrapper::~WebDataServiceWrapper() {
}

void WebDataServiceWrapper::Shutdown() {
  autofill_web_data_->ShutdownOnUIThread();
  keyword_web_data_->ShutdownOnUIThread();
  token_web_data_->ShutdownOnUIThread();

#if defined(OS_WIN)
  password_web_data_->ShutdownOnUIThread();
#endif
  web_database_->ShutdownDatabase();
}

scoped_refptr<AutofillWebDataService>
WebDataServiceWrapper::GetAutofillWebData() {
  return autofill_web_data_.get();
}

scoped_refptr<KeywordWebDataService>
WebDataServiceWrapper::GetKeywordWebData() {
  return keyword_web_data_.get();
}

scoped_refptr<TokenWebData> WebDataServiceWrapper::GetTokenWebData() {
  return token_web_data_.get();
}

#if defined(OS_WIN)
scoped_refptr<PasswordWebDataService>
WebDataServiceWrapper::GetPasswordWebData() {
  return password_web_data_.get();
}
#endif

WebDataServiceFactory::WebDataServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "WebDataService",
          BrowserContextDependencyManager::GetInstance()) {
  // WebDataServiceFactory has no dependecies.
}

WebDataServiceFactory::~WebDataServiceFactory() {
}

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForProfile(
    Profile* profile,
    Profile::ServiceAccessType access_type) {
  // If |access_type| starts being used for anything other than this
  // DCHECK, we need to start taking it as a parameter to
  // the *WebDataService::FromBrowserContext() functions (see above).
  DCHECK(access_type != Profile::IMPLICIT_ACCESS || !profile->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
WebDataServiceWrapper* WebDataServiceFactory::GetForProfileIfExists(
    Profile* profile,
    Profile::ServiceAccessType access_type) {
  // If |access_type| starts being used for anything other than this
  // DCHECK, we need to start taking it as a parameter to
  // the *WebDataService::FromBrowserContext() functions (see above).
  DCHECK(access_type != Profile::IMPLICIT_ACCESS || !profile->IsOffTheRecord());
  return static_cast<WebDataServiceWrapper*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
scoped_refptr<AutofillWebDataService>
WebDataServiceFactory::GetAutofillWebDataForProfile(
    Profile* profile,
    Profile::ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(profile, access_type);
  // |wrapper| can be NULL in Incognito mode.
  return wrapper ?
      wrapper->GetAutofillWebData() :
      scoped_refptr<AutofillWebDataService>(NULL);
}

// static
scoped_refptr<KeywordWebDataService>
WebDataServiceFactory::GetKeywordWebDataForProfile(
    Profile* profile,
    Profile::ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(profile, access_type);
  // |wrapper| can be NULL in Incognito mode.
  return wrapper ?
      wrapper->GetKeywordWebData() : scoped_refptr<KeywordWebDataService>(NULL);
}

// static
scoped_refptr<TokenWebData> WebDataServiceFactory::GetTokenWebDataForProfile(
    Profile* profile,
    Profile::ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(profile, access_type);
  // |wrapper| can be NULL in Incognito mode.
  return wrapper ?
      wrapper->GetTokenWebData() : scoped_refptr<TokenWebData>(NULL);
}

#if defined(OS_WIN)
// static
scoped_refptr<PasswordWebDataService>
WebDataServiceFactory::GetPasswordWebDataForProfile(
    Profile* profile,
    Profile::ServiceAccessType access_type) {
  WebDataServiceWrapper* wrapper =
      WebDataServiceFactory::GetForProfile(profile, access_type);
  // |wrapper| can be NULL in Incognito mode.
  return wrapper ?
      wrapper->GetPasswordWebData() :
      scoped_refptr<PasswordWebDataService>(NULL);
}
#endif

// static
WebDataServiceFactory* WebDataServiceFactory::GetInstance() {
  return Singleton<WebDataServiceFactory>::get();
}

content::BrowserContext* WebDataServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* WebDataServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new WebDataServiceWrapper(static_cast<Profile*>(profile));
}

bool WebDataServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
