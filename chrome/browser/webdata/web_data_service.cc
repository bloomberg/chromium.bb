// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/intents/default_web_intent_service.h"
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
#include "chrome/browser/webdata/web_database.h"
#include "chrome/browser/webdata/web_intents_table.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/form_field_data.h"
#ifdef DEBUG
#include "content/public/browser/browser_thread.h"
#endif
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

////////////////////////////////////////////////////////////////////////////////
//
// WebDataService implementation.
//
////////////////////////////////////////////////////////////////////////////////

using base::Bind;
using base::Time;
using content::BrowserThread;
using webkit_glue::WebIntentServiceData;

namespace {

// A task used by WebDataService (for Sync mainly) to inform the
// PersonalDataManager living on the UI thread that it needs to refresh.
void NotifyOfMultipleAutofillChangesTask(
    const scoped_refptr<WebDataService>& web_data_service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_MULTIPLE_CHANGED,
      content::Source<WebDataServiceBase>(web_data_service.get()),
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
      app_locale_(AutofillCountry::ApplicationLocale()),
      autocomplete_syncable_service_(NULL),
      autofill_profile_syncable_service_(NULL),
      failed_init_(false),
      should_commit_(false),
      main_loop_(MessageLoop::current()) {
  // WebDataService requires DB thread if instantiated.
  // Set WebDataServiceFactory::GetInstance()->SetTestingFactory(&profile, NULL)
  // if you do not want to instantiate WebDataService in your test.
  DCHECK(BrowserThread::IsWellKnownThread(BrowserThread::DB));
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

void WebDataService::ShutdownOnUIThread() {
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::ShutdownSyncableServices, this));
  UnloadDatabase();
}

bool WebDataService::Init(const FilePath& profile_path) {
  FilePath path = profile_path;
  path = path.Append(chrome::kWebDataFilename);
  return InitWithPath(path);
}

bool WebDataService::IsRunning() const {
  return is_running_;
}

void WebDataService::UnloadDatabase() {
  ScheduleTask(FROM_HERE, Bind(&WebDataService::ShutdownDatabase, this));
}

void WebDataService::CancelRequest(Handle h) {
  request_manager_.CancelRequest(h);
}

content::NotificationSource WebDataService::GetNotificationSource() {
  return content::Source<WebDataService>(this);
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

void WebDataService::AddKeyword(const TemplateURLData& data) {
  GenericRequest<TemplateURLData>* request =
      new GenericRequest<TemplateURLData>(
          this, NULL, &request_manager_, data);
  ScheduleTask(FROM_HERE, Bind(&WebDataService::AddKeywordImpl, this, request));
}

void WebDataService::RemoveKeyword(TemplateURLID id) {
  GenericRequest<TemplateURLID>* request =
      new GenericRequest<TemplateURLID>(this, NULL, &request_manager_, id);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveKeywordImpl, this, request));
}

void WebDataService::UpdateKeyword(const TemplateURLData& data) {
  GenericRequest<TemplateURLData>* request =
      new GenericRequest<TemplateURLData>(
          this, NULL, &request_manager_, data);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::UpdateKeywordImpl, this, request));
}

WebDataService::Handle WebDataService::GetKeywords(
                                       WebDataServiceConsumer* consumer) {
  WebDataRequest* request =
      new WebDataRequest(this, consumer, &request_manager_);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetKeywordsImpl, this, request));
  return request->GetHandle();
}

void WebDataService::SetDefaultSearchProvider(const TemplateURL* url) {
  GenericRequest<TemplateURLID>* request = new GenericRequest<TemplateURLID>(
      this, NULL, &request_manager_, url ? url->id() : 0);
  ScheduleTask(FROM_HERE, Bind(&WebDataService::SetDefaultSearchProviderImpl,
                               this, request));
}

void WebDataService::SetBuiltinKeywordVersion(int version) {
  GenericRequest<int>* request = new GenericRequest<int>(
      this, NULL, &request_manager_, version);
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
      new GenericRequest2<GURL, SkBitmap>(
          this, NULL, &request_manager_, app_url, image);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::SetWebAppImageImpl, this, request));
}

void WebDataService::SetWebAppHasAllImages(const GURL& app_url,
                                           bool has_all_images) {
  GenericRequest2<GURL, bool>* request =
      new GenericRequest2<GURL, bool>(
          this, NULL, &request_manager_, app_url, has_all_images);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::SetWebAppHasAllImagesImpl, this, request));
}

void WebDataService::RemoveWebApp(const GURL& app_url) {
  GenericRequest<GURL>* request =
      new GenericRequest<GURL>(this, NULL, &request_manager_, app_url);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveWebAppImpl, this, request));
}

WebDataService::Handle WebDataService::GetWebAppImages(
    const GURL& app_url,
    WebDataServiceConsumer* consumer) {
  GenericRequest<GURL>* request =
      new GenericRequest<GURL>(this, consumer, &request_manager_, app_url);
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
          this, NULL, &request_manager_, service);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::AddWebIntentServiceImpl, this, request));
}

void WebDataService::RemoveWebIntentService(
    const WebIntentServiceData& service) {
  GenericRequest<WebIntentServiceData>* request =
      new GenericRequest<WebIntentServiceData>(
          this, NULL, &request_manager_, service);
  ScheduleTask(FROM_HERE, Bind(&WebDataService::RemoveWebIntentServiceImpl,
                               this, request));
}

WebDataService::Handle WebDataService::GetWebIntentServicesForAction(
    const string16& action,
    WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  GenericRequest<string16>* request =
      new GenericRequest<string16>(
          this, consumer, &request_manager_, action);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetWebIntentServicesImpl, this, request));
  return request->GetHandle();
}

WebDataService::Handle WebDataService::GetWebIntentServicesForURL(
    const string16& service_url,
    WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  GenericRequest<string16>* request =
      new GenericRequest<string16>(
          this, consumer, &request_manager_, service_url);
  ScheduleTask(FROM_HERE, Bind(&WebDataService::GetWebIntentServicesForURLImpl,
                               this, request));
  return request->GetHandle();
}


WebDataService::Handle WebDataService::GetAllWebIntentServices(
    WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  GenericRequest<std::string>* request =
      new GenericRequest<std::string>(
          this, consumer, &request_manager_, std::string());
  ScheduleTask(FROM_HERE, Bind(&WebDataService::GetAllWebIntentServicesImpl,
                               this, request));
  return request->GetHandle();
}

void WebDataService::AddDefaultWebIntentService(
    const DefaultWebIntentService& service) {
  GenericRequest<DefaultWebIntentService>* request =
      new GenericRequest<DefaultWebIntentService>(
          this, NULL, &request_manager_, service);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::AddDefaultWebIntentServiceImpl, this,
                    request));
}

void WebDataService::RemoveDefaultWebIntentService(
    const DefaultWebIntentService& service) {
  GenericRequest<DefaultWebIntentService>* request =
      new GenericRequest<DefaultWebIntentService>(
          this, NULL, &request_manager_, service);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveDefaultWebIntentServiceImpl, this,
                    request));
}

void WebDataService::RemoveWebIntentServiceDefaults(
    const GURL& service_url) {
  GenericRequest<GURL>* request =
      new GenericRequest<GURL>(this, NULL, &request_manager_, service_url);
  ScheduleTask(
      FROM_HERE,
      Bind(&WebDataService::RemoveWebIntentServiceDefaultsImpl, this, request));
}

WebDataService::Handle WebDataService::GetDefaultWebIntentServicesForAction(
    const string16& action,
    WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  GenericRequest<string16>* request = new GenericRequest<string16>(
      this, consumer, &request_manager_, action);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetDefaultWebIntentServicesForActionImpl,
                    this, request));
  return request->GetHandle();
}

WebDataService::Handle WebDataService::GetAllDefaultWebIntentServices(
    WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  GenericRequest<std::string>* request = new GenericRequest<std::string>(
      this, consumer, &request_manager_, std::string());
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetAllDefaultWebIntentServicesImpl,
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
          this, NULL, &request_manager_, service, token);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::SetTokenForServiceImpl, this, request));
}

void WebDataService::RemoveAllTokens() {
  GenericRequest<std::string>* request =
      new GenericRequest<std::string>(
          this, NULL, &request_manager_, std::string());
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveAllTokensImpl, this, request));
}

// Null on failure. Success is WDResult<std::string>
WebDataService::Handle WebDataService::GetAllTokens(
    WebDataServiceConsumer* consumer) {

  GenericRequest<std::string>* request =
      new GenericRequest<std::string>(
          this, consumer, &request_manager_, std::string());
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetAllTokensImpl, this, request));
  return request->GetHandle();
}

////////////////////////////////////////////////////////////////////////////////
//
// Autofill.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::AddFormFields(
    const std::vector<FormFieldData>& fields) {
  GenericRequest<std::vector<FormFieldData> >* request =
      new GenericRequest<std::vector<FormFieldData> >(
          this, NULL, &request_manager_, fields);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::AddFormElementsImpl, this, request));
}

WebDataService::Handle WebDataService::GetFormValuesForElementName(
    const string16& name, const string16& prefix, int limit,
    WebDataServiceConsumer* consumer) {
  WebDataRequest* request =
      new WebDataRequest(this, consumer, &request_manager_);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetFormValuesForElementNameImpl,
                    this, request, name, prefix, limit));
  return request->GetHandle();
}

void WebDataService::RemoveFormElementsAddedBetween(const Time& delete_begin,
                                                    const Time& delete_end) {
  GenericRequest2<Time, Time>* request =
    new GenericRequest2<Time, Time>(
        this, NULL, &request_manager_, delete_begin, delete_end);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveFormElementsAddedBetweenImpl,
                    this, request));
}

void WebDataService::RemoveExpiredFormElements() {
  WebDataRequest* request =
      new WebDataRequest(this, NULL, &request_manager_);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveExpiredFormElementsImpl,
                    this, request));
}

void WebDataService::RemoveFormValueForElementName(
    const string16& name, const string16& value) {
  GenericRequest2<string16, string16>* request =
      new GenericRequest2<string16, string16>(
          this, NULL, &request_manager_, name, value);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveFormValueForElementNameImpl,
                    this, request));
}

void WebDataService::AddAutofillProfile(const AutofillProfile& profile) {
  GenericRequest<AutofillProfile>* request =
      new GenericRequest<AutofillProfile>(
          this, NULL, &request_manager_, profile);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::AddAutofillProfileImpl, this, request));
}

void WebDataService::UpdateAutofillProfile(const AutofillProfile& profile) {
  GenericRequest<AutofillProfile>* request =
      new GenericRequest<AutofillProfile>(
          this, NULL, &request_manager_, profile);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::UpdateAutofillProfileImpl, this, request));
}

void WebDataService::RemoveAutofillProfile(const std::string& guid) {
  GenericRequest<std::string>* request =
      new GenericRequest<std::string>(this, NULL, &request_manager_, guid);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveAutofillProfileImpl, this, request));
}

WebDataService::Handle WebDataService::GetAutofillProfiles(
    WebDataServiceConsumer* consumer) {
  WebDataRequest* request =
    new WebDataRequest(this, consumer, &request_manager_);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetAutofillProfilesImpl, this, request));
  return request->GetHandle();
}

void WebDataService::EmptyMigrationTrash(bool notify_sync) {
  GenericRequest<bool>* request =
      new GenericRequest<bool>(this, NULL, &request_manager_, notify_sync);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::EmptyMigrationTrashImpl, this, request));
}

void WebDataService::AddCreditCard(const CreditCard& credit_card) {
  GenericRequest<CreditCard>* request =
      new GenericRequest<CreditCard>(
          this, NULL, &request_manager_, credit_card);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::AddCreditCardImpl, this, request));
}

void WebDataService::UpdateCreditCard(const CreditCard& credit_card) {
  GenericRequest<CreditCard>* request =
      new GenericRequest<CreditCard>(
          this, NULL, &request_manager_, credit_card);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::UpdateCreditCardImpl, this, request));
}

void WebDataService::RemoveCreditCard(const std::string& guid) {
  GenericRequest<std::string>* request =
      new GenericRequest<std::string>(this, NULL, &request_manager_, guid);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::RemoveCreditCardImpl, this, request));
}

WebDataService::Handle WebDataService::GetCreditCards(
    WebDataServiceConsumer* consumer) {
  WebDataRequest* request =
      new WebDataRequest(this, consumer, &request_manager_);
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::GetCreditCardsImpl, this, request));
  return request->GetHandle();
}

void WebDataService::RemoveAutofillProfilesAndCreditCardsModifiedBetween(
    const Time& delete_begin,
    const Time& delete_end) {
  GenericRequest2<Time, Time>* request =
      new GenericRequest2<Time, Time>(
          this, NULL, &request_manager_, delete_begin, delete_end);
  ScheduleTask(FROM_HERE, Bind(
      &WebDataService::RemoveAutofillProfilesAndCreditCardsModifiedBetweenImpl,
      this, request));
}

WebDataService::~WebDataService() {
  if (is_running_ && db_) {
    DLOG_ASSERT("WebDataService dtor called without Shutdown");
    NOTREACHED();
  }
}

bool WebDataService::InitWithPath(const FilePath& path) {
  path_ = path;
  is_running_ = true;

  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::InitializeDatabaseIfNecessary, this));
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::InitializeSyncableServices, this));
  return true;
}

void WebDataService::RequestCompleted(Handle h) {
  request_manager_.RequestCompleted(h);
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
  sql::InitStatus init_status = db->Init(path_, app_locale_);
  if (init_status != sql::INIT_OK) {
    LOG(ERROR) << "Cannot initialize the web database: " << init_status;
    failed_init_ = true;
    delete db;
    if (main_loop_) {
      main_loop_->PostTask(
          FROM_HERE,
          base::Bind(&WebDataService::DBInitFailed, this, init_status));
    }
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&WebDataService::NotifyDatabaseLoadedOnUIThread, this));

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

////////////////////////////////////////////////////////////////////////////////
//
// Keywords implementation.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::AddKeywordImpl(GenericRequest<TemplateURLData>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    db_->GetKeywordTable()->AddKeyword(request->arg());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveKeywordImpl(GenericRequest<TemplateURLID>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    DCHECK(request->arg());
    db_->GetKeywordTable()->RemoveKeyword(request->arg());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::UpdateKeywordImpl(
    GenericRequest<TemplateURLData>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
    WDKeywordsResult result;
    db_->GetKeywordTable()->GetKeywords(&result.keywords);
    result.default_search_provider_id =
        db_->GetKeywordTable()->GetDefaultSearchProviderID();
    result.builtin_keyword_version =
        db_->GetKeywordTable()->GetBuiltinKeywordVersion();
    request->SetResult(
        new WDResult<WDKeywordsResult>(KEYWORDS_RESULT, result));
  }
  request->RequestComplete();
}

void WebDataService::SetDefaultSearchProviderImpl(
    GenericRequest<TemplateURLID>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
    db_->GetWebAppsTable()->SetWebAppImage(
        request->arg1(), request->arg2());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::SetWebAppHasAllImagesImpl(
    GenericRequest2<GURL, bool>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    db_->GetWebAppsTable()->SetWebAppHasAllImages(request->arg1(),
                                                  request->arg2());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveWebAppImpl(GenericRequest<GURL>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    db_->GetWebAppsTable()->RemoveWebApp(request->arg());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::GetWebAppImagesImpl(GenericRequest<GURL>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
    const WebIntentServiceData& service = request->arg();
    db_->GetWebIntentsTable()->RemoveWebIntentService(service);
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::AddWebIntentServiceImpl(
    GenericRequest<WebIntentServiceData>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    const WebIntentServiceData& service = request->arg();
    db_->GetWebIntentsTable()->SetWebIntentService(service);
    ScheduleCommit();
  }
  request->RequestComplete();
}


void WebDataService::GetWebIntentServicesImpl(
    GenericRequest<string16>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    std::vector<WebIntentServiceData> result;
    db_->GetWebIntentsTable()->GetWebIntentServicesForAction(request->arg(),
                                                             &result);
    request->SetResult(new WDResult<std::vector<WebIntentServiceData> >(
        WEB_INTENTS_RESULT, result));
  }
  request->RequestComplete();
}

void WebDataService::GetWebIntentServicesForURLImpl(
    GenericRequest<string16>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
    std::vector<WebIntentServiceData> result;
    db_->GetWebIntentsTable()->GetAllWebIntentServices(&result);
    request->SetResult(
        new WDResult<std::vector<WebIntentServiceData> >(
            WEB_INTENTS_RESULT, result));
  }
  request->RequestComplete();
}

void WebDataService::AddDefaultWebIntentServiceImpl(
    GenericRequest<DefaultWebIntentService>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    const DefaultWebIntentService& service = request->arg();
    db_->GetWebIntentsTable()->SetDefaultService(service);
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveDefaultWebIntentServiceImpl(
    GenericRequest<DefaultWebIntentService>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    const DefaultWebIntentService& service = request->arg();
    db_->GetWebIntentsTable()->RemoveDefaultService(service);
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveWebIntentServiceDefaultsImpl(
    GenericRequest<GURL>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    const GURL& service_url = request->arg();
    db_->GetWebIntentsTable()->RemoveServiceDefaults(service_url);
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::GetDefaultWebIntentServicesForActionImpl(
    GenericRequest<string16>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    std::vector<DefaultWebIntentService> result;
    db_->GetWebIntentsTable()->GetDefaultServices(
        request->arg(), &result);
    request->SetResult(
        new WDResult<std::vector<DefaultWebIntentService> >(
            WEB_INTENTS_DEFAULTS_RESULT, result));
  }
  request->RequestComplete();
}

void WebDataService::GetAllDefaultWebIntentServicesImpl(
    GenericRequest<std::string>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    std::vector<DefaultWebIntentService> result;
    db_->GetWebIntentsTable()->GetAllDefaultServices(&result);
    request->SetResult(
        new WDResult<std::vector<DefaultWebIntentService> >(
            WEB_INTENTS_DEFAULTS_RESULT, result));
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
  if (db_ && !request->IsCancelled()) {
    if (db_->GetTokenServiceTable()->RemoveAllTokens()) {
      ScheduleCommit();
    }
  }
  request->RequestComplete();
}

void WebDataService::SetTokenForServiceImpl(
    GenericRequest2<std::string, std::string>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
    std::map<std::string, std::string> map;
    db_->GetTokenServiceTable()->GetAllTokens(&map);
    request->SetResult(
        new WDResult<std::map<std::string, std::string> >(TOKEN_RESULT, map));
  }
  request->RequestComplete();
}

////////////////////////////////////////////////////////////////////////////////
//
// Autofill implementation.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::AddFormElementsImpl(
    GenericRequest<std::vector<FormFieldData> >* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
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

void WebDataService::RemoveExpiredFormElementsImpl(WebDataRequest* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    AutofillChangeList changes;
    if (db_->GetAutofillTable()->RemoveExpiredFormElements(&changes)) {
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
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
    std::vector<AutofillProfile*> profiles;
    db_->GetAutofillTable()->GetAutofillProfiles(&profiles);
    request->SetResult(
        new WDResult<std::vector<AutofillProfile*> >(AUTOFILL_PROFILES_RESULT,
            base::Bind(&WebDataService::DestroyAutofillProfileResult,
                base::Unretained(this)), profiles));
  }
  request->RequestComplete();
}

void WebDataService::EmptyMigrationTrashImpl(
    GenericRequest<bool>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
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
  if (db_ && !request->IsCancelled()) {
    std::vector<CreditCard*> credit_cards;
    db_->GetAutofillTable()->GetCreditCards(&credit_cards);
    request->SetResult(
        new WDResult<std::vector<CreditCard*> >(AUTOFILL_CREDITCARDS_RESULT,
             base::Bind(&WebDataService::DestroyAutofillCreditCardResult,
                base::Unretained(this)), credit_cards));
  }
  request->RequestComplete();
}

void WebDataService::RemoveAutofillProfilesAndCreditCardsModifiedBetweenImpl(
    GenericRequest2<Time, Time>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
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

void WebDataService::DestroyAutofillProfileResult(const WDTypedResult* result) {
  DCHECK(result->GetType() == AUTOFILL_PROFILES_RESULT);
  const WDResult<std::vector<AutofillProfile*> >* r =
      static_cast<const WDResult<std::vector<AutofillProfile*> >*>(result);
  std::vector<AutofillProfile*> profiles = r->GetValue();
  STLDeleteElements(&profiles);
}

void WebDataService::DestroyAutofillCreditCardResult(
      const WDTypedResult* result) {
  DCHECK(result->GetType() == AUTOFILL_CREDITCARDS_RESULT);
  const WDResult<std::vector<CreditCard*> >* r =
      static_cast<const WDResult<std::vector<CreditCard*> >*>(result);

  std::vector<CreditCard*> credit_cards = r->GetValue();
  STLDeleteElements(&credit_cards);
}
