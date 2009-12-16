// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_service.h"

#include "base/message_loop.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "webkit/glue/password_form.h"

////////////////////////////////////////////////////////////////////////////////
//
// WebDataService implementation.
//
////////////////////////////////////////////////////////////////////////////////

using base::Time;
using webkit_glue::FormField;
using webkit_glue::PasswordForm;

WebDataService::WebDataService() : thread_(NULL),
                                   db_(NULL),
                                   failed_init_(false),
                                   should_commit_(false),
                                   next_request_handle_(1) {
}

WebDataService::~WebDataService() {
  if (thread_) {
    Shutdown();
  }
}

bool WebDataService::Init(const FilePath& profile_path) {
  FilePath path = profile_path;
  path = path.Append(chrome::kWebDataFilename);
  return InitWithPath(path);
}

bool WebDataService::InitWithPath(const FilePath& path) {
  path_ = path;

  thread_ = new base::Thread("Chrome_WebDataThread");
  if (!thread_->Start()) {
    delete thread_;
    thread_ = NULL;
    return false;
  }

  ScheduleTask(NewRunnableMethod(this,
      &WebDataService::InitializeDatabaseIfNecessary));
  return true;
}

class ShutdownTask : public Task {
 public:
  explicit ShutdownTask(WebDataService* wds) : service_(wds) {
  }
  virtual void Run() {
    service_->ShutdownDatabase();
  }

 private:

  WebDataService* service_;
};

void WebDataService::Shutdown() {
  if (thread_) {
    // We cannot use NewRunnableMethod() because this can be called from our
    // destructor. NewRunnableMethod() would AddRef() this instance.
    ScheduleTask(new ShutdownTask(this));

    // The thread destructor sends a message to terminate the thread and waits
    // until the thread has exited.
    delete thread_;
    thread_ = NULL;
  }
}

bool WebDataService::IsRunning() const {
  return thread_ != NULL;
}

void WebDataService::UnloadDatabase() {
  ScheduleTask(NewRunnableMethod(this, &WebDataService::ShutdownDatabase));
}

void WebDataService::ScheduleCommit() {
  if (should_commit_ == false) {
    should_commit_ = true;
    ScheduleTask(NewRunnableMethod(this, &WebDataService::Commit));
  }
}

void WebDataService::ScheduleTask(Task* t) {
  if (thread_)
    thread_->message_loop()->PostTask(FROM_HERE, t);
  else
    NOTREACHED() << "Task scheduled after Shutdown()";
}

void WebDataService::RegisterRequest(WebDataRequest* request) {
  AutoLock l(pending_lock_);
  pending_requests_[request->GetHandle()] = request;
}

void WebDataService::CancelRequest(Handle h) {
  AutoLock l(pending_lock_);
  RequestMap::iterator i = pending_requests_.find(h);
  if (i == pending_requests_.end()) {
    NOTREACHED() << "Canceling a nonexistant web data service request";
    return;
  }
  i->second->Cancel();
}

void WebDataService::AddFormFieldValues(
    const std::vector<FormField>& element) {
  GenericRequest<std::vector<FormField> >* request =
      new GenericRequest<std::vector<FormField> >(
          this, GetNextRequestHandle(), NULL, element);
  RegisterRequest(request);
  ScheduleTask(NewRunnableMethod(this,
                                 &WebDataService::AddFormFieldValuesImpl,
                                 request));
}

WebDataService::Handle WebDataService::GetFormValuesForElementName(
    const string16& name, const string16& prefix, int limit,
    WebDataServiceConsumer* consumer) {
  WebDataRequest* request =
      new WebDataRequest(this, GetNextRequestHandle(), consumer);
  RegisterRequest(request);
  ScheduleTask(
      NewRunnableMethod(this,
                        &WebDataService::GetFormValuesForElementNameImpl,
                        request,
                        name,
                        prefix,
                        limit));
  return request->GetHandle();
}

void WebDataService::RemoveFormValueForElementName(
    const string16& name, const string16& value) {
  GenericRequest2<string16, string16>* request =
      new GenericRequest2<string16, string16>(this,
                                              GetNextRequestHandle(),
                                              NULL,
                                              name, value);
  RegisterRequest(request);
  ScheduleTask(
      NewRunnableMethod(this,
                        &WebDataService::RemoveFormValueForElementNameImpl,
                        request));
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
  WebDataServiceConsumer* consumer;
  if (!request->IsCancelled() && (consumer = request->GetConsumer())) {
    consumer->OnWebDataServiceRequestDone(request->GetHandle(),
                                          request->GetResult());
  }

  // If this is an autofill change request, post the notifications
  // including the list of affected keys.
  const WDTypedResult* result = request->GetResult();
  if (!request->IsCancelled() && result) {
    if (result->GetType() == AUTOFILL_CHANGES) {
      AutofillChangeList changes =
          static_cast<const WDResult<AutofillChangeList>*>(result)->GetValue();
      NotificationService::current()->Notify(
          NotificationType::AUTOFILL_ENTRIES_CHANGED,
          NotificationService::AllSources(),
          Details<AutofillChangeList>(&changes));
    }
  }
}

//////////////////////////////////////////////////////////////////////////////
//
// Keywords.
//
//////////////////////////////////////////////////////////////////////////////

void WebDataService::AddKeyword(const TemplateURL& url) {
  GenericRequest<TemplateURL>* request =
    new GenericRequest<TemplateURL>(this, GetNextRequestHandle(), NULL, url);
  RegisterRequest(request);
  ScheduleTask(NewRunnableMethod(this, &WebDataService::AddKeywordImpl,
                                 request));
}

void WebDataService::RemoveKeyword(const TemplateURL& url) {
  GenericRequest<TemplateURL::IDType>* request =
      new GenericRequest<TemplateURL::IDType>(this, GetNextRequestHandle(),
                                              NULL, url.id());
  RegisterRequest(request);
  ScheduleTask(
      NewRunnableMethod(this, &WebDataService::RemoveKeywordImpl, request));
}

void WebDataService::UpdateKeyword(const TemplateURL& url) {
  GenericRequest<TemplateURL>* request =
      new GenericRequest<TemplateURL>(this, GetNextRequestHandle(), NULL, url);
  RegisterRequest(request);
  ScheduleTask(
      NewRunnableMethod(this, &WebDataService::UpdateKeywordImpl, request));
}

WebDataService::Handle WebDataService::GetKeywords(
                                       WebDataServiceConsumer* consumer) {
  WebDataRequest* request =
      new WebDataRequest(this, GetNextRequestHandle(), consumer);
  RegisterRequest(request);
  ScheduleTask(
      NewRunnableMethod(this,
                        &WebDataService::GetKeywordsImpl,
                        request));
  return request->GetHandle();
}

void WebDataService::SetDefaultSearchProvider(const TemplateURL* url) {
  GenericRequest<TemplateURL::IDType>* request =
    new GenericRequest<TemplateURL::IDType>(this,
                                            GetNextRequestHandle(),
                                            NULL,
                                            url ? url->id() : 0);
  RegisterRequest(request);
  ScheduleTask(
      NewRunnableMethod(this, &WebDataService::SetDefaultSearchProviderImpl,
                        request));
}

void WebDataService::SetBuiltinKeywordVersion(int version) {
  GenericRequest<int>* request =
    new GenericRequest<int>(this, GetNextRequestHandle(), NULL, version);
  RegisterRequest(request);
  ScheduleTask(
      NewRunnableMethod(this, &WebDataService::SetBuiltinKeywordVersionImpl,
                        request));
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
  ScheduleTask(NewRunnableMethod(this, &WebDataService::SetWebAppImageImpl,
                                 request));
}

void WebDataService::SetWebAppHasAllImages(const GURL& app_url,
                                           bool has_all_images) {
  GenericRequest2<GURL, bool>* request =
      new GenericRequest2<GURL, bool>(this, GetNextRequestHandle(),
                                     NULL, app_url, has_all_images);
  RegisterRequest(request);
  ScheduleTask(NewRunnableMethod(this,
                                 &WebDataService::SetWebAppHasAllImagesImpl,
                                 request));
}

void WebDataService::RemoveWebApp(const GURL& app_url) {
  GenericRequest<GURL>* request =
      new GenericRequest<GURL>(this, GetNextRequestHandle(), NULL, app_url);
  RegisterRequest(request);
  ScheduleTask(NewRunnableMethod(this, &WebDataService::RemoveWebAppImpl,
                                 request));
}

WebDataService::Handle WebDataService::GetWebAppImages(
    const GURL& app_url,
    WebDataServiceConsumer* consumer) {
  GenericRequest<GURL>* request =
      new GenericRequest<GURL>(this, GetNextRequestHandle(), consumer, app_url);
  RegisterRequest(request);
  ScheduleTask(NewRunnableMethod(this, &WebDataService::GetWebAppImagesImpl,
                                 request));
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
  ScheduleTask(NewRunnableMethod(this, &WebDataService::AddLoginImpl,
                                 request));
}

void WebDataService::UpdateLogin(const PasswordForm& form) {
  GenericRequest<PasswordForm>* request =
      new GenericRequest<PasswordForm>(this, GetNextRequestHandle(),
                                       NULL, form);
  RegisterRequest(request);
  ScheduleTask(NewRunnableMethod(this, &WebDataService::UpdateLoginImpl,
                                 request));
}

void WebDataService::RemoveLogin(const PasswordForm& form) {
  GenericRequest<PasswordForm>* request =
     new GenericRequest<PasswordForm>(this, GetNextRequestHandle(), NULL,
                                      form);
  RegisterRequest(request);
  ScheduleTask(NewRunnableMethod(this, &WebDataService::RemoveLoginImpl,
                                 request));
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
  ScheduleTask(NewRunnableMethod(this,
      &WebDataService::RemoveLoginsCreatedBetweenImpl, request));
}

void WebDataService::RemoveLoginsCreatedAfter(const Time& delete_begin) {
  RemoveLoginsCreatedBetween(delete_begin, Time());
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
  ScheduleTask(NewRunnableMethod(this,
      &WebDataService::RemoveFormElementsAddedBetweenImpl, request));
}

WebDataService::Handle WebDataService::GetLogins(
                                       const PasswordForm& form,
                                       WebDataServiceConsumer* consumer) {
  GenericRequest<PasswordForm>* request =
      new GenericRequest<PasswordForm>(this, GetNextRequestHandle(),
                                       consumer, form);
  RegisterRequest(request);
  ScheduleTask(NewRunnableMethod(this, &WebDataService::GetLoginsImpl,
                                 request));
  return request->GetHandle();
}

WebDataService::Handle WebDataService::GetAutofillableLogins(
    WebDataServiceConsumer* consumer) {
  WebDataRequest* request =
      new WebDataRequest(this, GetNextRequestHandle(), consumer);
  RegisterRequest(request);
  ScheduleTask(NewRunnableMethod(this,
                                 &WebDataService::GetAutofillableLoginsImpl,
                                 request));
  return request->GetHandle();
}

WebDataService::Handle WebDataService::GetBlacklistLogins(
    WebDataServiceConsumer* consumer) {
  WebDataRequest* request =
      new WebDataRequest(this, GetNextRequestHandle(), consumer);
  RegisterRequest(request);
  ScheduleTask(NewRunnableMethod(this,
                                 &WebDataService::GetBlacklistLoginsImpl,
                                 request));
  return request->GetHandle();
}

////////////////////////////////////////////////////////////////////////////////
//
// The following methods are executed in Chrome_WebDataThread.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::Commit() {
  if (should_commit_) {
    should_commit_ = false;

    if (db_) {
      db_->CommitTransaction();
      db_->BeginTransaction();
    }
  }
}

void WebDataService::InitializeDatabaseIfNecessary() {
  if (db_ || failed_init_ || path_.empty())
    return;

  // In the rare case where the db fails to initialize a dialog may get shown
  // the blocks the caller, yet allows other messages through. For this reason
  // we only set db_ to the created database if creation is successful. That
  // way other methods won't do anything as db_ is still NULL.
  WebDatabase* db = new WebDatabase();
  if (!db->Init(path_)) {
    NOTREACHED() << "Cannot initialize the web database";
    failed_init_ = true;
    delete db;
    return;
  }

  db_ = db;
  db_->BeginTransaction();
}

void WebDataService::ShutdownDatabase() {
  should_commit_ = false;

  if (db_) {
    db_->CommitTransaction();
    delete db_;
    db_ = NULL;
  }
}

//
// Keywords.
//
void WebDataService::AddKeywordImpl(GenericRequest<TemplateURL>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    db_->AddKeyword(request->GetArgument());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveKeywordImpl(
    GenericRequest<TemplateURL::IDType>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    DCHECK(request->GetArgument());
    db_->RemoveKeyword(request->GetArgument());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::UpdateKeywordImpl(GenericRequest<TemplateURL>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    if (!db_->UpdateKeyword(request->GetArgument()))
      NOTREACHED();
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::GetKeywordsImpl(WebDataRequest* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    WDKeywordsResult result;
    db_->GetKeywords(&result.keywords);
    result.default_search_provider_id = db_->GetDefaulSearchProviderID();
    result.builtin_keyword_version = db_->GetBuitinKeywordVersion();
    request->SetResult(
        new WDResult<WDKeywordsResult>(KEYWORDS_RESULT, result));
  }
  request->RequestComplete();
}

void WebDataService::SetDefaultSearchProviderImpl(
    GenericRequest<TemplateURL::IDType>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    if (!db_->SetDefaultSearchProviderID(request->GetArgument()))
      NOTREACHED();
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::SetBuiltinKeywordVersionImpl(
    GenericRequest<int>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    if (!db_->SetBuitinKeywordVersion(request->GetArgument()))
      NOTREACHED();
    ScheduleCommit();
  }
  request->RequestComplete();
}

//
// Password manager support.
//
void WebDataService::AddLoginImpl(GenericRequest<PasswordForm>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    if (db_->AddLogin(request->GetArgument()))
      ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::UpdateLoginImpl(GenericRequest<PasswordForm>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    if (db_->UpdateLogin(request->GetArgument()))
      ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveLoginImpl(GenericRequest<PasswordForm>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    if (db_->RemoveLogin(request->GetArgument()))
      ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveLoginsCreatedBetweenImpl(
    GenericRequest2<Time, Time>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    if (db_->RemoveLoginsCreatedBetween(request->GetArgument1(),
                                        request->GetArgument2()))
      ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::GetLoginsImpl(GenericRequest<PasswordForm>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    std::vector<PasswordForm*> forms;
    db_->GetLogins(request->GetArgument(), &forms);
    request->SetResult(
        new WDResult<std::vector<PasswordForm*> >(PASSWORD_RESULT, forms));
  }
  request->RequestComplete();
}

void WebDataService::GetAutofillableLoginsImpl(WebDataRequest* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    std::vector<PasswordForm*> forms;
    db_->GetAllLogins(&forms, false);
    request->SetResult(
        new WDResult<std::vector<PasswordForm*> >(PASSWORD_RESULT, forms));
  }
  request->RequestComplete();
}

void WebDataService::GetBlacklistLoginsImpl(WebDataRequest* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    std::vector<PasswordForm*> all_forms;
    db_->GetAllLogins(&all_forms, true);
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
// Autofill support.
//
////////////////////////////////////////////////////////////////////////////////

void WebDataService::AddFormFieldValuesImpl(
    GenericRequest<std::vector<FormField> >* request) {
  InitializeDatabaseIfNecessary();
  const std::vector<FormField>& form_fields = request->GetArgument();
  if (db_ && !request->IsCancelled()) {
    if (!db_->AddFormFieldValues(form_fields))
      NOTREACHED();

    AutofillChangeList changes;
    for (std::vector<FormField>::const_iterator itr = form_fields.begin();
         itr != form_fields.end();
         itr++) {
      changes.push_back(
          AutofillChange(AutofillChange::ADD,
                         AutofillKey(itr->name(), itr->value())));
    }
    request->SetResult(
        new WDResult<AutofillChangeList>(AUTOFILL_CHANGES, changes));
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::GetFormValuesForElementNameImpl(WebDataRequest* request,
    const string16& name, const string16& prefix, int limit) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    std::vector<string16> values;
    db_->GetFormValuesForElementName(name, prefix, &values, limit);
    request->SetResult(
        new WDResult<std::vector<string16> >(AUTOFILL_VALUE_RESULT,
            values));
  }
  request->RequestComplete();
}

void WebDataService::RemoveFormElementsAddedBetweenImpl(
    GenericRequest2<Time, Time>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    if (db_->RemoveFormElementsAddedBetween(request->GetArgument1(),
                                            request->GetArgument2()))
      ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveFormValueForElementNameImpl(
    GenericRequest2<string16, string16>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    const string16& name = request->GetArgument1();
    const string16& value = request->GetArgument2();

    if (db_->RemoveFormElement(name, value)) {
      AutofillChangeList changes;
      changes.push_back(AutofillChange(AutofillChange::REMOVE,
                                       AutofillKey(name, value)));
      request->SetResult(
          new WDResult<AutofillChangeList>(AUTOFILL_CHANGES, changes));
      ScheduleCommit();
    }
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
    db_->SetWebAppImage(request->GetArgument1(), request->GetArgument2());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::SetWebAppHasAllImagesImpl(
    GenericRequest2<GURL, bool>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    db_->SetWebAppHasAllImages(request->GetArgument1(),
                               request->GetArgument2());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::RemoveWebAppImpl(GenericRequest<GURL>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    db_->RemoveWebApp(request->GetArgument());
    ScheduleCommit();
  }
  request->RequestComplete();
}

void WebDataService::GetWebAppImagesImpl(GenericRequest<GURL>* request) {
  InitializeDatabaseIfNecessary();
  if (db_ && !request->IsCancelled()) {
    WDAppImagesResult result;
    result.has_all_images = db_->GetWebAppHasAllImages(request->GetArgument());
    db_->GetWebAppImages(request->GetArgument(), &result.images);
    request->SetResult(
        new WDResult<WDAppImagesResult>(WEB_APP_IMAGES, result));
  }
  request->RequestComplete();
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
      canceled_(false),
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

WebDataServiceConsumer* WebDataService::WebDataRequest::GetConsumer() const {
  return consumer_;
}

bool WebDataService::WebDataRequest::IsCancelled() const {
  return canceled_;
}

void WebDataService::WebDataRequest::Cancel() {
  canceled_ = true;
  consumer_ = NULL;
}

void WebDataService::WebDataRequest::SetResult(WDTypedResult* r) {
  result_ = r;
}

const WDTypedResult* WebDataService::WebDataRequest::GetResult() const {
  return result_;
}

void WebDataService::WebDataRequest::RequestComplete() {
  WebDataService* s = service_;
  Task* t = NewRunnableMethod(s,
                              &WebDataService::RequestCompleted,
                              handle_);
  message_loop_->PostTask(FROM_HERE, t);
}

int WebDataService::GetNextRequestHandle() {
  AutoLock l(pending_lock_);
  return ++next_request_handle_;
}
