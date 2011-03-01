// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store.h"

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "content/browser/browser_thread.h"

using std::vector;
using webkit_glue::PasswordForm;

PasswordStore::PasswordStore() : handle_(0) {
}

bool PasswordStore::Init() {
  ReportMetrics();
  return true;
}

void PasswordStore::ScheduleTask(Task* task) {
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE, task);
}

void PasswordStore::ReportMetrics() {
  ScheduleTask(NewRunnableMethod(this, &PasswordStore::ReportMetricsImpl));
}

PasswordStore::~PasswordStore() {}

void PasswordStore::AddLogin(const PasswordForm& form) {
  ScheduleTask(NewRunnableMethod(this, &PasswordStore::AddLoginImpl, form));
}

void PasswordStore::UpdateLogin(const PasswordForm& form) {
  ScheduleTask(NewRunnableMethod(this, &PasswordStore::UpdateLoginImpl, form));
}

void PasswordStore::RemoveLogin(const PasswordForm& form) {
  ScheduleTask(NewRunnableMethod(this, &PasswordStore::RemoveLoginImpl, form));
}

void PasswordStore::RemoveLoginsCreatedBetween(const base::Time& delete_begin,
                                               const base::Time& delete_end) {
  ScheduleTask(NewRunnableMethod(this,
                                 &PasswordStore::RemoveLoginsCreatedBetweenImpl,
                                 delete_begin, delete_end));
}

int PasswordStore::GetLogins(const PasswordForm& form,
                             PasswordStoreConsumer* consumer) {
  int handle = GetNewRequestHandle();
  GetLoginsRequest* request = new GetLoginsRequest(consumer, handle);
  ScheduleTask(NewRunnableMethod(this, &PasswordStore::GetLoginsImpl, request,
                                 form));
  return handle;
}

int PasswordStore::GetAutofillableLogins(PasswordStoreConsumer* consumer) {
  int handle = GetNewRequestHandle();
  GetLoginsRequest* request = new GetLoginsRequest(consumer, handle);
  ScheduleTask(NewRunnableMethod(this,
                                 &PasswordStore::GetAutofillableLoginsImpl,
                                 request));
  return handle;
}

int PasswordStore::GetBlacklistLogins(PasswordStoreConsumer* consumer) {
  int handle = GetNewRequestHandle();
  GetLoginsRequest* request = new GetLoginsRequest(consumer, handle);
  ScheduleTask(NewRunnableMethod(this,
                                 &PasswordStore::GetBlacklistLoginsImpl,
                                 request));
  return handle;
}

void PasswordStore::NotifyConsumer(GetLoginsRequest* request,
                                   const vector<PasswordForm*> forms) {
  scoped_ptr<GetLoginsRequest> request_ptr(request);

#if !defined(OS_MACOSX)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
#endif
  request->message_loop->PostTask(FROM_HERE,
      NewRunnableMethod(this,
                        &PasswordStore::NotifyConsumerImpl,
                        request->consumer, request->handle, forms));
}

void PasswordStore::NotifyConsumerImpl(PasswordStoreConsumer* consumer,
                                       int handle,
                                       const vector<PasswordForm*> forms) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Don't notify the consumer if the request was canceled.
  if (pending_requests_.find(handle) == pending_requests_.end()) {
    // |forms| is const so we iterate rather than use STLDeleteElements().
    for (size_t i = 0; i < forms.size(); ++i)
      delete forms[i];
    return;
  }
  pending_requests_.erase(handle);

  consumer->OnPasswordStoreRequestDone(handle, forms);
}

int PasswordStore::GetNewRequestHandle() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int handle = handle_++;
  pending_requests_.insert(handle);
  return handle;
}

void PasswordStore::CancelLoginsQuery(int handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  pending_requests_.erase(handle);
}

PasswordStore::GetLoginsRequest::GetLoginsRequest(
    PasswordStoreConsumer* consumer, int handle)
    : consumer(consumer), handle(handle), message_loop(MessageLoop::current()) {
}
