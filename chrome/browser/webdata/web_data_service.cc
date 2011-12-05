// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_service.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/ui/profile_error_dialog.h"
#include "chrome/browser/webdata/autocomplete_syncable_service.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/autofill_profile_syncable_service.h"
#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/keyword_table.h"
#include "chrome/browser/webdata/logins_table.h"
#include "chrome/browser/webdata/token_service_table.h"
#include "chrome/browser/webdata/web_apps_table.h"
#include "chrome/browser/webdata/web_intents_table.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/password_form.h"

////////////////////////////////////////////////////////////////////////////////
//
// WebDataService implementation.
//
////////////////////////////////////////////////////////////////////////////////

using base::Bind;
using base::Time;
using content::BrowserThread;
using webkit_glue::FormField;
using webkit_glue::PasswordForm;
using webkit_glue::WebIntentServiceData;

namespace {

// A task used by WebDataService (for Sync mainly) to inform the
// PersonalDataManager living on the UI thread that it needs to refresh.
void NotifyOfMultipleAutofillChangesTask(
    const scoped_refptr<WebDataService>& web_data_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_MULTIPLE_CHANGED,
      content::Source<WebDataService>(web_data_service.get()),
      content::NotificationService::NoDetails());
}

}  // namespace

WDAppImagesResult::WDAppImagesResult() : has_all_images(false) {}

WDAppImagesResult::~WDAppImagesResult() {}

WDKeywordsResult::WDKeywordsResult()
  : default_search_provider_id(0),
    builtin_keyword_version(0) {
}

WDKeywordsResult::~WDKeywordsResult() {}

WebDataService::WebDataService()
  : is_running_(false),
    db_(NULL),
    autocomplete_syncable_service_(NULL),
    autofill_profile_syncable_service_(NULL),
    failed_init_(false),
    should_commit_(false),
    next_request_handle_(1),
    main_loop_(MessageLoop::current()) {
}

// static
void WebDataService::NotifyOfMultipleAutofillChanges(
    WebDataService* web_data_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  if (!web_data_service)
    return;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      Bind(&NotifyOfMultipleAutofillChangesTask,
           make_scoped_refptr(web_data_service)));
}

bool WebDataService::Init(const FilePath& profile_path) {
  FilePath path = profile_path;
  path = path.Append(chrome::kWebDataFilename);
  return InitWithPath(path);
}

void WebDataService::Shutdown() {
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::ShutdownSyncableServices, this));
  UnloadDatabase();
}

bool WebDataService::IsRunning() const {
  return is_running_;
}

void WebDataService::UnloadDatabase() {
  ScheduleTask(FROM_HERE, Bind(&WebDataService::ShutdownDatabase, this));
}

void WebDataService::CancelRequest(Handle h) {
  base::AutoLock l(pending_lock_);
  RequestMap::iterator i = pending_requests_.find(h);
  if (i == pending_requests_.end()) {
    NOTREACHED() << "Canceling a nonexistent web data service request";
    return;
  }
  i->second->Cancel();
}

bool WebDataService::IsDatabaseLoaded() {
  return db_ != NULL;
}

WebDatabase* WebDataService::GetDatabase() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  return db_;
}

//////////////////////////////////////////////////////////////////////////////
//
// Keywords.
//
//////////////////////////////////////////////////////////////////////////////

void WebDataService::AddKeyword(const TemplateURL& url) {
  // Ensure that the keyword is already generated (and cached) before caching
  // the TemplateURL for use on another keyword.
  url.EnsureKeyword();
  GenericRequest<TemplateURL>* request =
    new GenericRequest<TemplateURL>(this, GetNextRequestHandle(), NULL, url);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE, Bind(&WebDataService::AddKeywordImpl, this, request));
}

void WebDataService::RemoveKeyword(const TemplateURL& url) {
  GenericRequest<TemplateURLID>* request =
      new GenericRequest<TemplateURLID>(this, GetNextRequestHandle(),
                                        NULL, url.id());
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveKeywordImpl, this, request));
}

void WebDataService::UpdateKeyword(const TemplateURL& url) {
  // Ensure that the keyword is already generated (and cached) before caching
  // the TemplateURL for use on another keyword.
  url.EnsureKeyword();
  GenericRequest<TemplateURL>* request =
      new GenericRequest<TemplateURL>(this, GetNextRequestHandle(), NULL, url);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::UpdateKeywordImpl, this, request));
}

WebDataService::Handle WebDataService::GetKeywords(
                                       WebDataServiceConsumer* consumer) {
  WebDataRequest* request =
      new WebDataRequest(this, GetNextRequestHandle(), consumer);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetKeywordsImpl, this, request));
  return request->GetHandle();
}

void WebDataService::SetDefaultSearchProvider(const TemplateURL* url) {
  GenericRequest<TemplateURLID>* request =
    new GenericRequest<TemplateURLID>(this,
                                      GetNextRequestHandle(),
                                      NULL,
                                      url ? url->id() : 0);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE, Bind(&WebDataService::SetDefaultSearchProviderImpl,
                               this, request));
}

void WebDataService::SetBuiltinKeywordVersion(int version) {
  GenericRequest<int>* request =
    new GenericRequest<int>(this, GetNextRequestHandle(), NULL, version);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE, Bind(&WebDataService::SetBuiltinKeywordVersionImpl,
                               this, request));
}

//////////////////////////////////////////////////////////////////////////////
//
// Web Apps
//
//////////////////////////////////////////////////////////////////////////////

void WebDataService::SetWebAppImage(const GURL& app_url,
                                    const SkBitmap& image) {
  GenericRequest2<GURL, SkBitmap>* request =
      new GenericRequest2<GURL, SkBitmap>(this, GetNextRequestHandle(),
                                         NULL, app_url, image);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::SetWebAppImageImpl, this, request));
}

void WebDataService::SetWebAppHasAllImages(const GURL& app_url,
                                           bool has_all_images) {
  GenericRequest2<GURL, bool>* request =
      new GenericRequest2<GURL, bool>(this, GetNextRequestHandle(),
                                     NULL, app_url, has_all_images);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::SetWebAppHasAllImagesImpl, this, request));
}

void WebDataService::RemoveWebApp(const GURL& app_url) {
  GenericRequest<GURL>* request =
      new GenericRequest<GURL>(this, GetNextRequestHandle(), NULL, app_url);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveWebAppImpl, this, request));
}

WebDataService::Handle WebDataService::GetWebAppImages(
    const GURL& app_url,
    WebDataServiceConsumer* consumer) {
  GenericRequest<GURL>* request =
      new GenericRequest<GURL>(this, GetNextRequestHandle(), consumer, app_url);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetWebAppImagesImpl, this, request));
  return request->GetHandle();
}

//////////////////////////////////////////////////////////////////////////////
//
// Web Intents.
//
//////////////////////////////////////////////////////////////////////////////

void WebDataService::AddWebIntentService(const WebIntentServiceData& service) {
  GenericRequest<WebIntentServiceData>* request =
      new GenericRequest<WebIntentServiceData>(
          this, GetNextRequestHandle(), NULL, service);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::AddWebIntentServiceImpl, this, request));
}

void WebDataService::RemoveWebIntentService(
    const WebIntentServiceData& service) {
  GenericRequest<WebIntentServiceData>* request =
      new GenericRequest<WebIntentServiceData>(
          this, GetNextRequestHandle(), NULL, service);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE, Bind(&WebDataService::RemoveWebIntentServiceImpl,
                               this, request));
}

WebDataService::Handle WebDataService::GetWebIntentServices(
    const string16& action,
    WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  GenericRequest<string16>* request = new GenericRequest<string16>(
      this, GetNextRequestHandle(), consumer, action);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetWebIntentServicesImpl, this, request));
  return request->GetHandle();
}

WebDataService::Handle WebDataService::GetWebIntentServicesForURL(
    const string16& service_url,
    WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  GenericRequest<string16>* request = new GenericRequest<string16>(
          this, GetNextRequestHandle(), consumer, service_url);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE, Bind(&WebDataService::GetWebIntentServicesForURLImpl,
                               this, request));
  return request->GetHandle();
}


WebDataService::Handle WebDataService::GetAllWebIntentServices(
    WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  GenericRequest<std::string>* request = new GenericRequest<std::string>(
      this, GetNextRequestHandle(), consumer, std::string());
  RegisterRequest(request);
  ScheduleTask(FROM_HERE, Bind(&WebDataService::GetAllWebIntentServicesImpl,
                               this, request));
  return request->GetHandle();
}

////////////////////////////////////////////////////////////////////////////////
//
// Token Service
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::SetTokenForService(const std::string& service,
                                        const std::string& token) {
  GenericRequest2<std::string, std::string>* request =
      new GenericRequest2<std::string, std::string>(
          this, GetNextRequestHandle(), NULL, service, token);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::SetTokenForServiceImpl, this, request));
}

void WebDataService::RemoveAllTokens() {
  GenericRequest<std::string>* request =
      new GenericRequest<std::string>(
          this, GetNextRequestHandle(), NULL, std::string());
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveAllTokensImpl, this, request));
}

// Null on failure. Success is WDResult<std::string>
WebDataService::Handle WebDataService::GetAllTokens(
    WebDataServiceConsumer* consumer) {

  GenericRequest<std::string>* request =
      new GenericRequest<std::string>(
          this, GetNextRequestHandle(), consumer, std::string());
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetAllTokensImpl, this, request));
  return request->GetHandle();
}

////////////////////////////////////////////////////////////////////////////////
//
// Password manager.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::AddLogin(const PasswordForm& form) {
  GenericRequest<PasswordForm>* request =
      new GenericRequest<PasswordForm>(this, GetNextRequestHandle(), NULL,
                                       form);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE, Bind(&WebDataService::AddLoginImpl, this, request));
}

void WebDataService::UpdateLogin(const PasswordForm& form) {
  GenericRequest<PasswordForm>* request =
      new GenericRequest<PasswordForm>(this, GetNextRequestHandle(),
                                       NULL, form);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::UpdateLoginImpl, this, request));
}

void WebDataService::RemoveLogin(const PasswordForm& form) {
  GenericRequest<PasswordForm>* request =
     new GenericRequest<PasswordForm>(this, GetNextRequestHandle(), NULL,
                                      form);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveLoginImpl, this, request));
}

void WebDataService::RemoveLoginsCreatedBetween(const Time& delete_begin,
                                                const Time& delete_end) {
  GenericRequest2<Time, Time>* request =
    new GenericRequest2<Time, Time>(this,
                                    GetNextRequestHandle(),
                                    NULL,
                                    delete_begin,
                                    delete_end);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE, Bind(&WebDataService::RemoveLoginsCreatedBetweenImpl,
                               this, request));
}

void WebDataService::RemoveLoginsCreatedAfter(const Time& delete_begin) {
  RemoveLoginsCreatedBetween(delete_begin, Time());
}

WebDataService::Handle WebDataService::GetLogins(
                                       const PasswordForm& form,
                                       WebDataServiceConsumer* consumer) {
  GenericRequest<PasswordForm>* request =
      new GenericRequest<PasswordForm>(this, GetNextRequestHandle(),
                                       consumer, form);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE, Bind(&WebDataService::GetLoginsImpl, this, request));
  return request->GetHandle();
}

WebDataService::Handle WebDataService::GetAutofillableLogins(
    WebDataServiceConsumer* consumer) {
  WebDataRequest* request =
      new WebDataRequest(this, GetNextRequestHandle(), consumer);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetAutofillableLoginsImpl, this, request));
  return request->GetHandle();
}

WebDataService::Handle WebDataService::GetBlacklistLogins(
    WebDataServiceConsumer* consumer) {
  WebDataRequest* request =
      new WebDataRequest(this, GetNextRequestHandle(), consumer);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetBlacklistLoginsImpl, this, request));
  return request->GetHandle();
}

////////////////////////////////////////////////////////////////////////////////
//
// Autofill.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::AddFormFields(
    const std::vector<FormField>& fields) {
  GenericRequest<std::vector<FormField> >* request =
      new GenericRequest<std::vector<FormField> >(
          this, GetNextRequestHandle(), NULL, fields);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::AddFormElementsImpl, this, request));
}

WebDataService::Handle WebDataService::GetFormValuesForElementName(
    const string16& name, const string16& prefix, int limit,
    WebDataServiceConsumer* consumer) {
  WebDataRequest* request =
      new WebDataRequest(this, GetNextRequestHandle(), consumer);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetFormValuesForElementNameImpl,
                    this, request, name, prefix, limit));
  return request->GetHandle();
}

void WebDataService::RemoveFormElementsAddedBetween(const Time& delete_begin,
                                                    const Time& delete_end) {
  GenericRequest2<Time, Time>* request =
    new GenericRequest2<Time, Time>(this,
                                    GetNextRequestHandle(),
                                    NULL,
                                    delete_begin,
                                    delete_end);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveFormElementsAddedBetweenImpl,
                    this, request));
}

void WebDataService::RemoveFormValueForElementName(
    const string16& name, const string16& value) {
  GenericRequest2<string16, string16>* request =
      new GenericRequest2<string16, string16>(this,
                                              GetNextRequestHandle(),
                                              NULL,
                                              name, value);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveFormValueForElementNameImpl,
                    this, request));
}

void WebDataService::AddAutofillProfile(const AutofillProfile& profile) {
  GenericRequest<AutofillProfile>* request =
      new GenericRequest<AutofillProfile>(
          this, GetNextRequestHandle(), NULL, profile);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::AddAutofillProfileImpl, this, request));
}

void WebDataService::UpdateAutofillProfile(const AutofillProfile& profile) {
  GenericRequest<AutofillProfile>* request =
      new GenericRequest<AutofillProfile>(
          this, GetNextRequestHandle(), NULL, profile);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::UpdateAutofillProfileImpl, this, request));
}

void WebDataService::RemoveAutofillProfile(const std::string& guid) {
  GenericRequest<std::string>* request =
      new GenericRequest<std::string>(
          this, GetNextRequestHandle(), NULL, guid);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveAutofillProfileImpl, this, request));
}

WebDataService::Handle WebDataService::GetAutofillProfiles(
    WebDataServiceConsumer* consumer) {
  WebDataRequest* request =
      new WebDataRequest(this, GetNextRequestHandle(), consumer);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetAutofillProfilesImpl, this, request));
  return request->GetHandle();
}

void WebDataService::EmptyMigrationTrash(bool notify_sync) {
  GenericRequest<bool>* request =
      new GenericRequest<bool>(
          this, GetNextRequestHandle(), NULL, notify_sync);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::EmptyMigrationTrashImpl, this, request));
}

void WebDataService::AddCreditCard(const CreditCard& credit_card) {
  GenericRequest<CreditCard>* request =
      new GenericRequest<CreditCard>(
          this, GetNextRequestHandle(), NULL, credit_card);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::AddCreditCardImpl, this, request));
}

void WebDataService::UpdateCreditCard(const CreditCard& credit_card) {
  GenericRequest<CreditCard>* request =
      new GenericRequest<CreditCard>(
          this, GetNextRequestHandle(), NULL, credit_card);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::UpdateCreditCardImpl, this, request));
}

void WebDataService::RemoveCreditCard(const std::string& guid) {
  GenericRequest<std::string>* request =
      new GenericRequest<std::string>(
          this, GetNextRequestHandle(), NULL, guid);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveCreditCardImpl, this, request));
}

WebDataService::Handle WebDataService::GetCreditCards(
    WebDataServiceConsumer* consumer) {
  WebDataRequest* request =
      new WebDataRequest(this, GetNextRequestHandle(), consumer);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetCreditCardsImpl, this, request));
  return request->GetHandle();
}

void WebDataService::RemoveAutofillProfilesAndCreditCardsModifiedBetween(
    const Time& delete_begin,
    const Time& delete_end) {
  GenericRequest2<Time, Time>* request =
      new GenericRequest2<Time, Time>(this,
                                      GetNextRequestHandle(),
                                      NULL,
                                      delete_begin,
                                      delete_end);
  RegisterRequest(request);
  ScheduleTask(FROM_HERE, Bind(
      &WebDataService::RemoveAutofillProfilesAndCreditCardsModifiedBetweenImpl,
      this, request));
}

WebDataService::~WebDataService() {
  if (is_running_ && db_) {
    DLOG_ASSERT("WebDataService dtor called without Shutdown");
  }
}

bool WebDataService::InitWithPath(const FilePath& path) {
  path_ = path;
  is_running_ = true;

  // TODO(isherman): For now, to avoid a data race on shutdown
  // [ http://crbug.com/100745 ], call |AutofillCountry::ApplicationLocale()| to
  // cache the application locale before we try to access it on the DB thread.
  // This should be safe to remove once [ http://crbug.com/100845 ] is fixed.
  AutofillCountry::ApplicationLocale();

  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::InitializeDatabaseIfNecessary, this));
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::InitializeSyncableServices, this));
  return true;
}

void WebDataService::RequestCompleted(Handle h) {
  pending_lock_.Acquire();
  RequestMap::iterator i = pending_requests_.find(h);
  if (i == pending_requests_.end()) {
    NOTREACHED() << "Request completed called for an unknown request";
    pending_lock_.Release();
    return;
  }

  // Take ownership of the request object and remove it from the map.
  scoped_ptr<WebDataRequest> request(i->second);
  pending_requests_.erase(i);
  pending_lock_.Release();

  // Notify the consumer if needed.
  WebDataServiceConsumer* consumer = NULL;
  if (!request->IsCancelled(&consumer) && consumer) {
    consumer->OnWebDataServiceRequestDone(request->GetHandle(),
                                          request->GetResult());
  } else {
    // Nobody is taken ownership of the result, either because it is cancelled
    // or there is no consumer. Destroy results that require special handling.
    WDTypedResult const *result = request->GetResult();
    if (result) {
      if (result->GetType() == AUTOFILL_PROFILES_RESULT) {
        const WDResult<std::vector<AutofillProfile*> >* r =
            static_cast<const WDResult<std::vector<AutofillProfile*> >*>(
                result);
        std::vector<AutofillProfile*> profiles = r->GetValue();
        STLDeleteElements(&profiles);
      } else if (result->GetType() == AUTOFILL_CREDITCARDS_RESULT) {
        const WDResult<std::vector<CreditCard*> >* r =
            static_cast<const WDResult<std::vector<CreditCard*> >*>(result);

        std::vector<CreditCard*> credit_cards = r->GetValue();
        STLDeleteElements(&credit_cards);
      }
    }
  }
}

void WebDataService::RegisterRequest(WebDataRequest* request) {
  base::AutoLock l(pending_lock_);
  pending_requests_[request->GetHandle()] = request;
}

////////////////////////////////////////////////////////////////////////////////
//
// The following methods are executed in Chrome_WebDataThread.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::DBInitFailed(sql::InitStatus init_status) {
  ShowProfileErrorDialog(
      (init_status == sql::INIT_FAILURE) ?
      IDS_COULDNT_OPEN_PROFILE_ERROR : IDS_PROFILE_TOO_NEW_ERROR);
}

void WebDataService::InitializeDatabaseIfNecessary() {
  if (db_ || failed_init_ || path_.empty())
    return;

  // In the rare case where the db fails to initialize a dialog may get shown
  // that blocks the caller, yet allows other messages through. For this reason
  // we only set db_ to the created database if creation is successful. That
  // way other methods won't do anything as db_ is still NULL.
  WebDatabase* db = new WebDatabase();
  sql::InitStatus init_status = db->Init(path_);
  if (init_status != sql::INIT_OK) {
    LOG(ERROR) << "Cannot initialize the web database: " << init_status;
    failed_init_ = true;
    delete db;
    if (main_loop_) {
      main_loop_->PostTask(FROM_HERE,
          NewRunnableMethod(this, &WebDataService::DBInitFailed, init_status));
    }
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &WebDataService::NotifyDatabaseLoadedOnUIThread));

  db_ = db;
  db_->BeginTransaction();
}

void WebDataService::InitializeSyncableServices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK(!autocomplete_syncable_service_);
  DCHECK(!autofill_profile_syncable_service_);

  autocomplete_syncable_service_ = new AutocompleteSyncableService(this);
  autofill_profile_syncable_service_ = new AutofillProfileSyncableService(this);
}

void WebDataService::NotifyDatabaseLoadedOnUIThread() {
  // Notify that the database has been initialized.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_WEB_DATABASE_LOADED,
      content::Source<WebDataService>(this),
      content::NotificationService::NoDetails());
}

void WebDataService::ShutdownDatabase() {
  should_commit_ = false;

  if (db_) {
    db_->CommitTransaction();
    delete db_;
    db_ = NULL;
  }
}

void WebDataService::ShutdownSyncableServices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  delete autocomplete_syncable_service_;
  autocomplete_syncable_service_ = NULL;
  delete autofill_profile_syncable_service_;
  autofill_profile_syncable_service_ = NULL;
}

void WebDataService::Commit() {
  if (should_commit_) {
    should_commit_ = false;

    if (db_) {
      db_->CommitTransaction();
      db_->BeginTransaction();
    }
  }
}

void WebDataService::ScheduleTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task) {
  if (is_running_)
    BrowserThread::PostTask(BrowserThread::DB, from_here, task);
  else
    NOTREACHED() << "Task scheduled after Shutdown()";
}

void WebDataService::ScheduleCommit() {
  if (should_commit_ == false) {
    should_commit_ = true;
    ScheduleTask(FROM_HERE, Bind(&WebDataService::Commit, this));
  }
}

int WebDataService::GetNextRequestHandle() {
  base::AutoLock l(pending_lock_);
  return ++next_request_handle_;
}

////////////////////////////////////////////////////////////////////////////////
//
// Keywords implementation.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::AddKeywordImpl(GenericRequest<TemplateURL>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    db_->GetKeywordTable()->AddKeyword(request->arg());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveKeywordImpl(
    GenericRequest<TemplateURLID>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    DCHECK(request->arg());
    db_->GetKeywordTable()->RemoveKeyword(request->arg());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::UpdateKeywordImpl(GenericRequest<TemplateURL>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    if (!db_->GetKeywordTable()->UpdateKeyword(request->arg())) {
      NOTREACHED();
      return;
    }
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::GetKeywordsImpl(WebDataRequest* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    WDKeywordsResult result;
    db_->GetKeywordTable()->GetKeywords(&result.keywords);
    result.default_search_provider_id =
        db_->GetKeywordTable()->GetDefaultSearchProviderID();
    result.builtin_keyword_version =
        db_->GetKeywordTable()->GetBuiltinKeywordVersion();
    result.default_search_provider_id_backup =
        db_->GetKeywordTable()->GetDefaultSearchProviderIDBackup();
    result.did_default_search_provider_change =
        db_->GetKeywordTable()->DidDefaultSearchProviderChange();
    request->SetResult(
        new WDResult<WDKeywordsResult>(KEYWORDS_RESULT, result));
  }
  request->RequestComplete();
}

void WebDataService::SetDefaultSearchProviderImpl(
    GenericRequest<TemplateURLID>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    if (!db_->GetKeywordTable()->SetDefaultSearchProviderID(request->arg())) {
      NOTREACHED();
      return;
    }
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::SetBuiltinKeywordVersionImpl(
    GenericRequest<int>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    if (!db_->GetKeywordTable()->SetBuiltinKeywordVersion(request->arg())) {
      NOTREACHED();
      return;
    }
    ScheduleCommit();
  }
  request->RequestComplete();
}

////////////////////////////////////////////////////////////////////////////////
//
// Web Apps implementation.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::SetWebAppImageImpl(
    GenericRequest2<GURL, SkBitmap>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    db_->GetWebAppsTable()->SetWebAppImage(
        request->arg1(), request->arg2());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::SetWebAppHasAllImagesImpl(
    GenericRequest2<GURL, bool>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    db_->GetWebAppsTable()->SetWebAppHasAllImages(request->arg1(),
                                                  request->arg2());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveWebAppImpl(GenericRequest<GURL>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    db_->GetWebAppsTable()->RemoveWebApp(request->arg());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::GetWebAppImagesImpl(GenericRequest<GURL>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    WDAppImagesResult result;
    result.has_all_images =
        db_->GetWebAppsTable()->GetWebAppHasAllImages(request->arg());
    db_->GetWebAppsTable()->GetWebAppImages(request->arg(), &result.images);
    request->SetResult(
        new WDResult<WDAppImagesResult>(WEB_APP_IMAGES, result));
  }
  request->RequestComplete();
}

////////////////////////////////////////////////////////////////////////////////
//
// Web Intents implementation.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::RemoveWebIntentServiceImpl(
    GenericRequest<WebIntentServiceData>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    const WebIntentServiceData& service = request->arg();
    db_->GetWebIntentsTable()->RemoveWebIntentService(service);
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::AddWebIntentServiceImpl(
    GenericRequest<WebIntentServiceData>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    const WebIntentServiceData& service = request->arg();
    db_->GetWebIntentsTable()->SetWebIntentService(service);
    ScheduleCommit();
  }
  request->RequestComplete();
}


void WebDataService::GetWebIntentServicesImpl(
    GenericRequest<string16>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    std::vector<WebIntentServiceData> result;
    db_->GetWebIntentsTable()->GetWebIntentServices(request->arg(), &result);
    request->SetResult(
        new WDResult<std::vector<WebIntentServiceData> >(
            WEB_INTENTS_RESULT, result));
  }
  request->RequestComplete();
}

void WebDataService::GetWebIntentServicesForURLImpl(
    GenericRequest<string16>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    std::vector<WebIntentServiceData> result;
    db_->GetWebIntentsTable()->GetWebIntentServicesForURL(
        request->arg(), &result);
    request->SetResult(
        new WDResult<std::vector<WebIntentServiceData> >(
            WEB_INTENTS_RESULT, result));
  }
  request->RequestComplete();
}

void WebDataService::GetAllWebIntentServicesImpl(
    GenericRequest<std::string>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    std::vector<WebIntentServiceData> result;
    db_->GetWebIntentsTable()->GetAllWebIntentServices(&result);
    request->SetResult(
        new WDResult<std::vector<WebIntentServiceData> >(
            WEB_INTENTS_RESULT, result));
  }
  request->RequestComplete();
}

////////////////////////////////////////////////////////////////////////////////
//
// Token Service implementation.
//
////////////////////////////////////////////////////////////////////////////////

// argument std::string is unused
void WebDataService::RemoveAllTokensImpl(
    GenericRequest<std::string>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    if (db_->GetTokenServiceTable()->RemoveAllTokens()) {
      ScheduleCommit();
    }
  }
  request->RequestComplete();
}

void WebDataService::SetTokenForServiceImpl(
    GenericRequest2<std::string, std::string>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    if (db_->GetTokenServiceTable()->SetTokenForService(
            request->arg1(), request->arg2())) {
      ScheduleCommit();
    }
  }
  request->RequestComplete();
}

// argument is unused
void WebDataService::GetAllTokensImpl(
    GenericRequest<std::string>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    std::map<std::string, std::string> map;
    db_->GetTokenServiceTable()->GetAllTokens(&map);
    request->SetResult(
        new WDResult<std::map<std::string, std::string> >(TOKEN_RESULT, map));
  }
  request->RequestComplete();
}

////////////////////////////////////////////////////////////////////////////////
//
// Password manager implementation.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::AddLoginImpl(GenericRequest<PasswordForm>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    if (db_->GetLoginsTable()->AddLogin(request->arg()))
      ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::UpdateLoginImpl(GenericRequest<PasswordForm>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    if (db_->GetLoginsTable()->UpdateLogin(request->arg()))
      ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveLoginImpl(GenericRequest<PasswordForm>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    if (db_->GetLoginsTable()->RemoveLogin(request->arg()))
      ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveLoginsCreatedBetweenImpl(
    GenericRequest2<Time, Time>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    if (db_->GetLoginsTable()->RemoveLoginsCreatedBetween(
            request->arg1(), request->arg2())) {
      ScheduleCommit();
    }
  }
  request->RequestComplete();
}

void WebDataService::GetLoginsImpl(GenericRequest<PasswordForm>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    std::vector<PasswordForm*> forms;
    db_->GetLoginsTable()->GetLogins(request->arg(), &forms);
    request->SetResult(
        new WDResult<std::vector<PasswordForm*> >(PASSWORD_RESULT, forms));
  }
  request->RequestComplete();
}

void WebDataService::GetAutofillableLoginsImpl(WebDataRequest* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    std::vector<PasswordForm*> forms;
    db_->GetLoginsTable()->GetAllLogins(&forms, false);
    request->SetResult(
        new WDResult<std::vector<PasswordForm*> >(PASSWORD_RESULT, forms));
  }
  request->RequestComplete();
}

void WebDataService::GetBlacklistLoginsImpl(WebDataRequest* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    std::vector<PasswordForm*> all_forms;
    db_->GetLoginsTable()->GetAllLogins(&all_forms, true);
    std::vector<PasswordForm*> blacklist_forms;
    for (std::vector<PasswordForm*>::iterator i = all_forms.begin();
         i != all_forms.end(); ++i) {
      scoped_ptr<PasswordForm> form(*i);
      if (form->blacklisted_by_user) {
        blacklist_forms.push_back(form.release());
      }
    }
    all_forms.clear();
    request->SetResult(
        new WDResult<std::vector<PasswordForm*> >(PASSWORD_RESULT,
                                                  blacklist_forms));
  }
  request->RequestComplete();
}

////////////////////////////////////////////////////////////////////////////////
//
// Autofill implementation.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::AddFormElementsImpl(
    GenericRequest<std::vector<FormField> >* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    AutofillChangeList changes;
    if (!db_->GetAutofillTable()->AddFormFieldValues(
            request->arg(), &changes)) {
      NOTREACHED();
      return;
    }
    request->SetResult(
        new WDResult<AutofillChangeList>(AUTOFILL_CHANGES, changes));
    ScheduleCommit();

    // Post the notifications including the list of affected keys.
    // This is sent here so that work resulting from this notification will be
    // done on the DB thread, and not the UI thread.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
        content::Source<WebDataService>(this),
        content::Details<AutofillChangeList>(&changes));
  }

  request->RequestComplete();
}

void WebDataService::GetFormValuesForElementNameImpl(WebDataRequest* request,
    const string16& name, const string16& prefix, int limit) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    std::vector<string16> values;
    db_->GetAutofillTable()->GetFormValuesForElementName(
        name, prefix, &values, limit);
    request->SetResult(
        new WDResult<std::vector<string16> >(AUTOFILL_VALUE_RESULT, values));
  }
  request->RequestComplete();
}

void WebDataService::RemoveFormElementsAddedBetweenImpl(
    GenericRequest2<Time, Time>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    AutofillChangeList changes;
    if (db_->GetAutofillTable()->RemoveFormElementsAddedBetween(
        request->arg1(), request->arg2(), &changes)) {
      if (!changes.empty()) {
        request->SetResult(
            new WDResult<AutofillChangeList>(AUTOFILL_CHANGES, changes));

        // Post the notifications including the list of affected keys.
        // This is sent here so that work resulting from this notification
        // will be done on the DB thread, and not the UI thread.
        content::NotificationService::current()->Notify(
            chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
            content::Source<WebDataService>(this),
            content::Details<AutofillChangeList>(&changes));
      }
      ScheduleCommit();
    }
  }
  request->RequestComplete();
}

void WebDataService::RemoveFormValueForElementNameImpl(
    GenericRequest2<string16, string16>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    const string16& name = request->arg1();
    const string16& value = request->arg2();

    if (db_->GetAutofillTable()->RemoveFormElement(name, value)) {
      AutofillChangeList changes;
      changes.push_back(AutofillChange(AutofillChange::REMOVE,
                                       AutofillKey(name, value)));
      request->SetResult(
          new WDResult<AutofillChangeList>(AUTOFILL_CHANGES, changes));
      ScheduleCommit();

      // Post the notifications including the list of affected keys.
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
          content::Source<WebDataService>(this),
          content::Details<AutofillChangeList>(&changes));
    }
  }
  request->RequestComplete();
}

void WebDataService::AddAutofillProfileImpl(
    GenericRequest<AutofillProfile>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    const AutofillProfile& profile = request->arg();
    if (!db_->GetAutofillTable()->AddAutofillProfile(profile)) {
      NOTREACHED();
      return;
    }
    ScheduleCommit();

    // Send GUID-based notification.
    AutofillProfileChange change(AutofillProfileChange::ADD,
                                 profile.guid(), &profile);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
        content::Source<WebDataService>(this),
        content::Details<AutofillProfileChange>(&change));
  }
  request->RequestComplete();
}

void WebDataService::UpdateAutofillProfileImpl(
    GenericRequest<AutofillProfile>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    const AutofillProfile& profile = request->arg();

    // Only perform the update if the profile exists.  It is currently
    // valid to try to update a missing profile.  We simply drop the write and
    // the caller will detect this on the next refresh.
    AutofillProfile* original_profile = NULL;
    if (!db_->GetAutofillTable()->GetAutofillProfile(profile.guid(),
                                                     &original_profile)) {
      request->RequestComplete();
      return;
    }
    scoped_ptr<AutofillProfile> scoped_profile(original_profile);

    if (!db_->GetAutofillTable()->UpdateAutofillProfileMulti(profile)) {
      NOTREACHED();
      return;
    }
    ScheduleCommit();

    // Send GUID-based notification.
    AutofillProfileChange change(AutofillProfileChange::UPDATE,
                                 profile.guid(), &profile);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
        content::Source<WebDataService>(this),
        content::Details<AutofillProfileChange>(&change));
  }
  request->RequestComplete();
}

void WebDataService::RemoveAutofillProfileImpl(
    GenericRequest<std::string>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    const std::string& guid = request->arg();

    AutofillProfile* profile = NULL;
    if (!db_->GetAutofillTable()->GetAutofillProfile(guid, &profile)) {
      NOTREACHED();
      return;
    }
    scoped_ptr<AutofillProfile> scoped_profile(profile);

    if (!db_->GetAutofillTable()->RemoveAutofillProfile(guid)) {
      NOTREACHED();
      return;
    }
    ScheduleCommit();

    // Send GUID-based notification.
    AutofillProfileChange change(AutofillProfileChange::REMOVE, guid, NULL);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
        content::Source<WebDataService>(this),
        content::Details<AutofillProfileChange>(&change));
  }
  request->RequestComplete();
}

void WebDataService::GetAutofillProfilesImpl(WebDataRequest* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    std::vector<AutofillProfile*> profiles;
    db_->GetAutofillTable()->GetAutofillProfiles(&profiles);
    request->SetResult(
        new WDResult<std::vector<AutofillProfile*> >(AUTOFILL_PROFILES_RESULT,
                                                     profiles));
  }
  request->RequestComplete();
}

void WebDataService::EmptyMigrationTrashImpl(
    GenericRequest<bool>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    bool notify_sync = request->arg();
    if (notify_sync) {
      std::vector<std::string> guids;
      if (!db_->GetAutofillTable()->GetAutofillProfilesInTrash(&guids)) {
        NOTREACHED();
        return;
      }

      for (std::vector<std::string>::const_iterator iter = guids.begin();
           iter != guids.end(); ++iter) {
        // Send GUID-based notification.
        AutofillProfileChange change(AutofillProfileChange::REMOVE,
                                     *iter, NULL);
        content::NotificationService::current()->Notify(
            chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
            content::Source<WebDataService>(this),
            content::Details<AutofillProfileChange>(&change));
      }

      // If we trashed any profiles they may have been merged, so send out
      // update notifications as well.
      if (!guids.empty()) {
        std::vector<AutofillProfile*> profiles;
        db_->GetAutofillTable()->GetAutofillProfiles(&profiles);
        for (std::vector<AutofillProfile*>::const_iterator
                iter = profiles.begin();
             iter != profiles.end(); ++iter) {
          AutofillProfileChange change(AutofillProfileChange::UPDATE,
                                       (*iter)->guid(), *iter);
          content::NotificationService::current()->Notify(
              chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
              content::Source<WebDataService>(this),
              content::Details<AutofillProfileChange>(&change));
        }
        STLDeleteElements(&profiles);
      }
    }

    if (!db_->GetAutofillTable()->EmptyAutofillProfilesTrash()) {
      NOTREACHED();
      return;
    }
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::AddCreditCardImpl(
    GenericRequest<CreditCard>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    const CreditCard& credit_card = request->arg();
    if (!db_->GetAutofillTable()->AddCreditCard(credit_card)) {
      NOTREACHED();
      return;
    }
    ScheduleCommit();

    // Send GUID-based notification.
    AutofillCreditCardChange change(AutofillCreditCardChange::ADD,
                                    credit_card.guid(), &credit_card);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_AUTOFILL_CREDIT_CARD_CHANGED,
        content::Source<WebDataService>(this),
        content::Details<AutofillCreditCardChange>(&change));
  }
  request->RequestComplete();
}

void WebDataService::UpdateCreditCardImpl(
    GenericRequest<CreditCard>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    const CreditCard& credit_card = request->arg();

    // It is currently valid to try to update a missing profile.  We simply drop
    // the write and the caller will detect this on the next refresh.
    CreditCard* original_credit_card = NULL;
    if (!db_->GetAutofillTable()->GetCreditCard(credit_card.guid(),
                                                &original_credit_card)) {
      request->RequestComplete();
      return;
    }
    scoped_ptr<CreditCard> scoped_credit_card(original_credit_card);

    if (!db_->GetAutofillTable()->UpdateCreditCard(credit_card)) {
      NOTREACHED();
      return;
    }
    ScheduleCommit();

    // Send GUID-based notification.
    AutofillCreditCardChange change(AutofillCreditCardChange::UPDATE,
                                    credit_card.guid(), &credit_card);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_AUTOFILL_CREDIT_CARD_CHANGED,
        content::Source<WebDataService>(this),
        content::Details<AutofillCreditCardChange>(&change));
  }
  request->RequestComplete();
}

void WebDataService::RemoveCreditCardImpl(
    GenericRequest<std::string>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    const std::string& guid = request->arg();
    if (!db_->GetAutofillTable()->RemoveCreditCard(guid)) {
      NOTREACHED();
      return;
    }
    ScheduleCommit();

    // Send GUID-based notification.
    AutofillCreditCardChange change(AutofillCreditCardChange::REMOVE, guid,
                                    NULL);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_AUTOFILL_CREDIT_CARD_CHANGED,
        content::Source<WebDataService>(this),
        content::Details<AutofillCreditCardChange>(&change));
  }
  request->RequestComplete();
}

void WebDataService::GetCreditCardsImpl(WebDataRequest* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    std::vector<CreditCard*> credit_cards;
    db_->GetAutofillTable()->GetCreditCards(&credit_cards);
    request->SetResult(
        new WDResult<std::vector<CreditCard*> >(AUTOFILL_CREDITCARDS_RESULT,
                                                credit_cards));
  }
  request->RequestComplete();
}

void WebDataService::RemoveAutofillProfilesAndCreditCardsModifiedBetweenImpl(
    GenericRequest2<Time, Time>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled(NULL)) {
    std::vector<std::string> profile_guids;
    std::vector<std::string> credit_card_guids;
    if (db_->GetAutofillTable()->
        RemoveAutofillProfilesAndCreditCardsModifiedBetween(
            request->arg1(),
            request->arg2(),
            &profile_guids,
            &credit_card_guids)) {
      for (std::vector<std::string>::iterator iter = profile_guids.begin();
           iter != profile_guids.end(); ++iter) {
        AutofillProfileChange change(AutofillProfileChange::REMOVE, *iter,
                                     NULL);
        content::NotificationService::current()->Notify(
            chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
            content::Source<WebDataService>(this),
            content::Details<AutofillProfileChange>(&change));
      }

      for (std::vector<std::string>::iterator iter = credit_card_guids.begin();
           iter != credit_card_guids.end(); ++iter) {
        AutofillCreditCardChange change(AutofillCreditCardChange::REMOVE,
                                        *iter, NULL);
        content::NotificationService::current()->Notify(
            chrome::NOTIFICATION_AUTOFILL_CREDIT_CARD_CHANGED,
            content::Source<WebDataService>(this),
            content::Details<AutofillCreditCardChange>(&change));
      }
      // Note: It is the caller's responsibility to post notifications for any
      // changes, e.g. by calling the Refresh() method of PersonalDataManager.
      ScheduleCommit();
    }
  }
  request->RequestComplete();
}

AutofillProfileSyncableService*
    WebDataService::GetAutofillProfileSyncableService() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK(autofill_profile_syncable_service_);  // Make sure we're initialized.

  return autofill_profile_syncable_service_;
}

AutocompleteSyncableService* WebDataService::GetAutocompleteSyncableService()
    const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  DCHECK(autocomplete_syncable_service_);  // Make sure we're initialized.

  return autocomplete_syncable_service_;
}


////////////////////////////////////////////////////////////////////////////////
//
// WebDataRequest implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDataService::WebDataRequest::WebDataRequest(WebDataService* service,
                                               Handle handle,
                                               WebDataServiceConsumer* consumer)
    : service_(service),
      handle_(handle),
      cancelled_(false),
      consumer_(consumer),
      result_(NULL) {
  message_loop_ = MessageLoop::current();
}

WebDataService::WebDataRequest::~WebDataRequest() {
  delete result_;
}

WebDataService::Handle WebDataService::WebDataRequest::GetHandle() const {
  return handle_;
}

bool WebDataService::WebDataRequest::IsCancelled(
    WebDataServiceConsumer** consumer) const {
  base::AutoLock l(cancel_lock_);
  if (consumer)
    *consumer = consumer_;
  return cancelled_;
}

void WebDataService::WebDataRequest::Cancel() {
  base::AutoLock l(cancel_lock_);
  cancelled_ = true;
  consumer_ = NULL;
}

void WebDataService::WebDataRequest::SetResult(WDTypedResult* r) {
  result_ = r;
}

const WDTypedResult* WebDataService::WebDataRequest::GetResult() const {
  return result_;
}

void WebDataService::WebDataRequest::RequestComplete() {
  message_loop_->PostTask(FROM_HERE, Bind(&WebDataService::RequestCompleted,
                                          service_.get(), handle_));
}
