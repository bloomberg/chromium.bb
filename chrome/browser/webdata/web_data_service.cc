// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
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
      request_manager_(new WebDataRequestManager()),
      app_locale_(AutofillCountry::ApplicationLocale()),
      autocomplete_syncable_service_(NULL),
      autofill_profile_syncable_service_(NULL),
      failed_init_(false),
      should_commit_(false),
      main_loop_(MessageLoop::current()) {
  // WebDataService requires DB thread if instantiated.
  // Set WebDataServiceFactory::GetInstance()->SetTestingFactory(&profile, NULL)
  // if you do not want to instantiate WebDataService in your test.
  DCHECK(!ProfileManager::IsImportProcess(*CommandLine::ForCurrentProcess()));
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

bool WebDataService::Init(const base::FilePath& profile_path) {
  base::FilePath path = profile_path;
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
  request_manager_->CancelRequest(h);
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
  ScheduleDBTask(
      FROM_HERE, Bind(&WebDataService::AddKeywordImpl, this, data));
}

void WebDataService::RemoveKeyword(TemplateURLID id) {
  ScheduleDBTask(
      FROM_HERE, Bind(&WebDataService::RemoveKeywordImpl, this, id));
}

void WebDataService::UpdateKeyword(const TemplateURLData& data) {
  ScheduleDBTask(
      FROM_HERE, Bind(&WebDataService::UpdateKeywordImpl, this, data));
}

WebDataService::Handle WebDataService::GetKeywords(
    WebDataServiceConsumer* consumer) {
  return ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetKeywordsImpl, this), consumer);
}

void WebDataService::SetDefaultSearchProvider(const TemplateURL* url) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetDefaultSearchProviderImpl, this,
           url ? url->id() : 0));
}

void WebDataService::SetBuiltinKeywordVersion(int version) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetBuiltinKeywordVersionImpl, this, version));
}

//////////////////////////////////////////////////////////////////////////////
//
// Web Apps
//
//////////////////////////////////////////////////////////////////////////////

void WebDataService::SetWebAppImage(const GURL& app_url,
                                    const SkBitmap& image) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetWebAppImageImpl, this, app_url, image));
}

void WebDataService::SetWebAppHasAllImages(const GURL& app_url,
                                           bool has_all_images) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetWebAppHasAllImagesImpl, this, app_url,
           has_all_images));
}

void WebDataService::RemoveWebApp(const GURL& app_url) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveWebAppImpl, this, app_url));
}

WebDataService::Handle WebDataService::GetWebAppImages(
    const GURL& app_url,
    WebDataServiceConsumer* consumer) {
  return ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetWebAppImagesImpl, this, app_url), consumer);
}

////////////////////////////////////////////////////////////////////////////////
//
// Token Service
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::SetTokenForService(const std::string& service,
                                        const std::string& token) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::SetTokenForServiceImpl, this, service, token));
}

void WebDataService::RemoveAllTokens() {
  ScheduleDBTask(FROM_HERE, Bind(&WebDataService::RemoveAllTokensImpl, this));
}

// Null on failure. Success is WDResult<std::string>
WebDataService::Handle WebDataService::GetAllTokens(
    WebDataServiceConsumer* consumer) {
  return ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetAllTokensImpl, this), consumer);
}

////////////////////////////////////////////////////////////////////////////////
//
// Autofill.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::AddFormFields(
    const std::vector<FormFieldData>& fields) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::AddFormElementsImpl, this, fields));
}

WebDataService::Handle WebDataService::GetFormValuesForElementName(
    const string16& name, const string16& prefix, int limit,
    WebDataServiceConsumer* consumer) {
  return ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetFormValuesForElementNameImpl,
           this, name, prefix, limit),
      consumer);
}

void WebDataService::RemoveFormElementsAddedBetween(const Time& delete_begin,
                                                    const Time& delete_end) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveFormElementsAddedBetweenImpl,
           this, delete_begin, delete_end));
}

void WebDataService::RemoveExpiredFormElements() {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveExpiredFormElementsImpl, this));
}

void WebDataService::RemoveFormValueForElementName(
    const string16& name, const string16& value) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveFormValueForElementNameImpl,
           this, name, value));
}

void WebDataService::AddAutofillProfile(const AutofillProfile& profile) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::AddAutofillProfileImpl, this, profile));
}

void WebDataService::UpdateAutofillProfile(const AutofillProfile& profile) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::UpdateAutofillProfileImpl, this, profile));
}

void WebDataService::RemoveAutofillProfile(const std::string& guid) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveAutofillProfileImpl, this, guid));
}

WebDataService::Handle WebDataService::GetAutofillProfiles(
    WebDataServiceConsumer* consumer) {
  return ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetAutofillProfilesImpl, this), consumer);
}

void WebDataService::EmptyMigrationTrash(bool notify_sync) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::EmptyMigrationTrashImpl, this, notify_sync));
}

void WebDataService::AddCreditCard(const CreditCard& credit_card) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::AddCreditCardImpl, this, credit_card));
}

void WebDataService::UpdateCreditCard(const CreditCard& credit_card) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::UpdateCreditCardImpl, this, credit_card));
}

void WebDataService::RemoveCreditCard(const std::string& guid) {
  ScheduleDBTask(FROM_HERE,
      Bind(&WebDataService::RemoveCreditCardImpl, this, guid));
}

WebDataService::Handle WebDataService::GetCreditCards(
    WebDataServiceConsumer* consumer) {
  return ScheduleDBTaskWithResult(FROM_HERE,
      Bind(&WebDataService::GetCreditCardsImpl, this), consumer);
}

void WebDataService::RemoveAutofillProfilesAndCreditCardsModifiedBetween(
    const Time& delete_begin,
    const Time& delete_end) {
  ScheduleDBTask(FROM_HERE, Bind(
      &WebDataService::RemoveAutofillProfilesAndCreditCardsModifiedBetweenImpl,
      this, delete_begin, delete_end));
}

WebDataService::~WebDataService() {
  if (is_running_ && db_) {
    NOTREACHED() << "WebDataService dtor called without Shutdown";
  }
}

bool WebDataService::InitWithPath(const base::FilePath& path) {
  path_ = path;
  is_running_ = true;

  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::InitializeDatabaseIfNecessary, this));
  ScheduleTask(FROM_HERE,
               Bind(&WebDataService::InitializeSyncableServices, this));
  return true;
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

void WebDataService::ScheduleDBTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task) {
  scoped_ptr<WebDataRequest> request(
      new WebDataRequest(NULL, request_manager_.get()));
  if (is_running_) {
    BrowserThread::PostTask(BrowserThread::DB, from_here,
        base::Bind(&WebDataService::DBTaskWrapper, this, task,
                   base::Passed(&request)));
  } else {
    NOTREACHED() << "Task scheduled after Shutdown()";
  }
}

WebDataService::Handle WebDataService::ScheduleDBTaskWithResult(
      const tracked_objects::Location& from_here,
      const ResultTask& task,
      WebDataServiceConsumer* consumer) {
  DCHECK(consumer);
  scoped_ptr<WebDataRequest> request(
      new WebDataRequest(consumer, request_manager_.get()));
  WebDataService::Handle handle = request->GetHandle();
  if (is_running_) {
    BrowserThread::PostTask(BrowserThread::DB, from_here,
        base::Bind(&WebDataService::DBResultTaskWrapper, this, task,
                   base::Passed(&request)));
  } else {
    NOTREACHED() << "Task scheduled after Shutdown()";
  }
  return handle;
}

void WebDataService::DBTaskWrapper(const base::Closure& task,
                                   scoped_ptr<WebDataRequest> request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    task.Run();
  }
  request_manager_->RequestCompleted(request.Pass());
}

void WebDataService::DBResultTaskWrapper(const ResultTask& task,
                                         scoped_ptr<WebDataRequest> request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    request->SetResult(task.Run());
  }
  request_manager_->RequestCompleted(request.Pass());
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

void WebDataService::AddKeywordImpl(const TemplateURLData& data) {
  db_->GetKeywordTable()->AddKeyword(data);
  ScheduleCommit();
}

void WebDataService::RemoveKeywordImpl(TemplateURLID id) {
  DCHECK(id);
  db_->GetKeywordTable()->RemoveKeyword(id);
  ScheduleCommit();
}

void WebDataService::UpdateKeywordImpl(const TemplateURLData& data) {
  if (!db_->GetKeywordTable()->UpdateKeyword(data)) {
    NOTREACHED();
    return;
  }
  ScheduleCommit();
}

scoped_ptr<WDTypedResult> WebDataService::GetKeywordsImpl() {
  WDKeywordsResult result;
  db_->GetKeywordTable()->GetKeywords(&result.keywords);
  result.default_search_provider_id =
      db_->GetKeywordTable()->GetDefaultSearchProviderID();
  result.builtin_keyword_version =
      db_->GetKeywordTable()->GetBuiltinKeywordVersion();
  return scoped_ptr<WDTypedResult>(
      new WDResult<WDKeywordsResult>(KEYWORDS_RESULT, result));
}

void WebDataService::SetDefaultSearchProviderImpl(TemplateURLID id) {
  if (!db_->GetKeywordTable()->SetDefaultSearchProviderID(id)) {
    NOTREACHED();
    return;
  }
  ScheduleCommit();
}

void WebDataService::SetBuiltinKeywordVersionImpl(int version) {
  if (!db_->GetKeywordTable()->SetBuiltinKeywordVersion(version)) {
    NOTREACHED();
    return;
  }
  ScheduleCommit();
}

////////////////////////////////////////////////////////////////////////////////
//
// Web Apps implementation.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::SetWebAppImageImpl(
    const GURL& app_url, const SkBitmap& image) {
  db_->GetWebAppsTable()->SetWebAppImage(app_url, image);
  ScheduleCommit();
}

void WebDataService::SetWebAppHasAllImagesImpl(
    const GURL& app_url, bool has_all_images) {
  db_->GetWebAppsTable()->SetWebAppHasAllImages(app_url, has_all_images);
  ScheduleCommit();
}

void WebDataService::RemoveWebAppImpl(const GURL& app_url) {
  db_->GetWebAppsTable()->RemoveWebApp(app_url);
  ScheduleCommit();
}

scoped_ptr<WDTypedResult> WebDataService::GetWebAppImagesImpl(
    const GURL& app_url) {
  WDAppImagesResult result;
  result.has_all_images =
      db_->GetWebAppsTable()->GetWebAppHasAllImages(app_url);
  db_->GetWebAppsTable()->GetWebAppImages(app_url, &result.images);
  return scoped_ptr<WDTypedResult>(
      new WDResult<WDAppImagesResult>(WEB_APP_IMAGES, result));
}

////////////////////////////////////////////////////////////////////////////////
//
// Token Service implementation.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::RemoveAllTokensImpl() {
  if (db_->GetTokenServiceTable()->RemoveAllTokens()) {
    ScheduleCommit();
  }
}

void WebDataService::SetTokenForServiceImpl(const std::string& service,
                                            const std::string& token) {
  if (db_->GetTokenServiceTable()->SetTokenForService(service, token)) {
    ScheduleCommit();
  }
}

scoped_ptr<WDTypedResult> WebDataService::GetAllTokensImpl() {
  std::map<std::string, std::string> map;
  db_->GetTokenServiceTable()->GetAllTokens(&map);
  return scoped_ptr<WDTypedResult>(
      new WDResult<std::map<std::string, std::string> >(TOKEN_RESULT, map));
}

////////////////////////////////////////////////////////////////////////////////
//
// Autofill implementation.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::AddFormElementsImpl(
    const std::vector<FormFieldData>& fields) {
  AutofillChangeList changes;
  if (!db_->GetAutofillTable()->AddFormFieldValues(fields, &changes)) {
    NOTREACHED();
    return;
  }
  ScheduleCommit();

  // Post the notifications including the list of affected keys.
  // This is sent here so that work resulting from this notification will be
  // done on the DB thread, and not the UI thread.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
      content::Source<WebDataService>(this),
      content::Details<AutofillChangeList>(&changes));
}

scoped_ptr<WDTypedResult> WebDataService::GetFormValuesForElementNameImpl(
  const string16& name, const string16& prefix, int limit) {
  std::vector<string16> values;
  db_->GetAutofillTable()->GetFormValuesForElementName(
      name, prefix, &values, limit);
  return scoped_ptr<WDTypedResult>(
      new WDResult<std::vector<string16> >(AUTOFILL_VALUE_RESULT, values));
}

void WebDataService::RemoveFormElementsAddedBetweenImpl(
  const base::Time& delete_begin, const base::Time& delete_end) {
  AutofillChangeList changes;
  if (db_->GetAutofillTable()->RemoveFormElementsAddedBetween(
      delete_begin, delete_end, &changes)) {
    if (!changes.empty()) {
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

void WebDataService::RemoveExpiredFormElementsImpl() {
  AutofillChangeList changes;

  if (db_->GetAutofillTable()->RemoveExpiredFormElements(&changes)) {
    if (!changes.empty()) {
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

void WebDataService::RemoveFormValueForElementNameImpl(
    const string16& name, const string16& value) {

  if (db_->GetAutofillTable()->RemoveFormElement(name, value)) {
    AutofillChangeList changes;
    changes.push_back(AutofillChange(AutofillChange::REMOVE,
                                     AutofillKey(name, value)));
    ScheduleCommit();

    // Post the notifications including the list of affected keys.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
        content::Source<WebDataService>(this),
        content::Details<AutofillChangeList>(&changes));
  }
}

void WebDataService::AddAutofillProfileImpl(const AutofillProfile& profile) {
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

void WebDataService::UpdateAutofillProfileImpl(const AutofillProfile& profile) {
  // Only perform the update if the profile exists.  It is currently
  // valid to try to update a missing profile.  We simply drop the write and
  // the caller will detect this on the next refresh.
  AutofillProfile* original_profile = NULL;
  if (!db_->GetAutofillTable()->GetAutofillProfile(profile.guid(),
                                                   &original_profile)) {
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

void WebDataService::RemoveAutofillProfileImpl(const std::string& guid) {
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

scoped_ptr<WDTypedResult> WebDataService::GetAutofillProfilesImpl() {
  std::vector<AutofillProfile*> profiles;
  db_->GetAutofillTable()->GetAutofillProfiles(&profiles);
  return scoped_ptr<WDTypedResult>(
      new WDDestroyableResult<std::vector<AutofillProfile*> >(
          AUTOFILL_PROFILES_RESULT,
          profiles,
          base::Bind(&WebDataService::DestroyAutofillProfileResult,
              base::Unretained(this))));
}

void WebDataService::EmptyMigrationTrashImpl(bool notify_sync) {
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

void WebDataService::AddCreditCardImpl(const CreditCard& credit_card) {
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

void WebDataService::UpdateCreditCardImpl(const CreditCard& credit_card) {
  // It is currently valid to try to update a missing profile.  We simply drop
  // the write and the caller will detect this on the next refresh.
  CreditCard* original_credit_card = NULL;
  if (!db_->GetAutofillTable()->GetCreditCard(credit_card.guid(),
                                              &original_credit_card)) {
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

void WebDataService::RemoveCreditCardImpl(const std::string& guid) {
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

scoped_ptr<WDTypedResult> WebDataService::GetCreditCardsImpl() {
  std::vector<CreditCard*> credit_cards;
  db_->GetAutofillTable()->GetCreditCards(&credit_cards);
  return scoped_ptr<WDTypedResult>(
      new WDDestroyableResult<std::vector<CreditCard*> >(
          AUTOFILL_CREDITCARDS_RESULT,
          credit_cards,
          base::Bind(&WebDataService::DestroyAutofillCreditCardResult,
              base::Unretained(this))));
}

void WebDataService::RemoveAutofillProfilesAndCreditCardsModifiedBetweenImpl(
  const base::Time& delete_begin, const base::Time& delete_end) {
  std::vector<std::string> profile_guids;
  std::vector<std::string> credit_card_guids;
  if (db_->GetAutofillTable()->
      RemoveAutofillProfilesAndCreditCardsModifiedBetween(
          delete_begin,
          delete_end,
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
