// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/password_manager/password_store_consumer.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/forms/password_form.h"

using content::BrowserThread;
using std::vector;
using webkit::forms::PasswordForm;

PasswordStore::GetLoginsRequest::GetLoginsRequest(
    const GetLoginsCallback& callback)
    : CancelableRequest1<GetLoginsCallback,
                         std::vector<PasswordForm*> >(callback) {
}

PasswordStore::GetLoginsRequest::~GetLoginsRequest() {
  if (canceled()) {
    STLDeleteElements(&value);
  }
}

PasswordStore::PasswordStore() {
}

bool PasswordStore::Init() {
  ReportMetrics();
  return true;
}

void PasswordStore::AddLogin(const PasswordForm& form) {
  ScheduleTask(base::Bind(&PasswordStore::WrapModificationTask, this,
      base::Closure(base::Bind(&PasswordStore::AddLoginImpl, this, form))));
}

void PasswordStore::UpdateLogin(const PasswordForm& form) {
  ScheduleTask(base::Bind(&PasswordStore::WrapModificationTask, this,
      base::Closure(base::Bind(&PasswordStore::UpdateLoginImpl, this, form))));
}

void PasswordStore::RemoveLogin(const PasswordForm& form) {
  ScheduleTask(base::Bind(&PasswordStore::WrapModificationTask, this,
      base::Closure(base::Bind(&PasswordStore::RemoveLoginImpl, this, form))));
}

void PasswordStore::RemoveLoginsCreatedBetween(const base::Time& delete_begin,
                                               const base::Time& delete_end) {
  ScheduleTask(base::Bind(&PasswordStore::WrapModificationTask, this,
      base::Closure(
          base::Bind(&PasswordStore::RemoveLoginsCreatedBetweenImpl, this,
                     delete_begin, delete_end))));
}

CancelableRequestProvider::Handle PasswordStore::GetLogins(
    const PasswordForm& form, PasswordStoreConsumer* consumer) {
  return Schedule(&PasswordStore::GetLoginsImpl, consumer, form);
}

CancelableRequestProvider::Handle PasswordStore::GetAutofillableLogins(
    PasswordStoreConsumer* consumer) {
  return Schedule(&PasswordStore::GetAutofillableLoginsImpl, consumer);
}

CancelableRequestProvider::Handle PasswordStore::GetBlacklistLogins(
    PasswordStoreConsumer* consumer) {
  return Schedule(&PasswordStore::GetBlacklistLoginsImpl, consumer);
}

void PasswordStore::ReportMetrics() {
  ScheduleTask(base::Bind(&PasswordStore::ReportMetricsImpl, this));
}

void PasswordStore::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PasswordStore::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

PasswordStore::~PasswordStore() {}

PasswordStore::GetLoginsRequest* PasswordStore::NewGetLoginsRequest(
    const GetLoginsCallback& callback) {
  return new GetLoginsRequest(callback);
}

bool PasswordStore::ScheduleTask(const base::Closure& task) {
  return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE, task);
}

void PasswordStore::ForwardLoginsResult(GetLoginsRequest* request) {
  request->ForwardResult(request->handle(), request->value);
}

template<typename BackendFunc>
CancelableRequestProvider::Handle PasswordStore::Schedule(
    BackendFunc func, PasswordStoreConsumer* consumer) {
  scoped_refptr<GetLoginsRequest> request(NewGetLoginsRequest(
      base::Bind(&PasswordStoreConsumer::OnPasswordStoreRequestDone,
                 base::Unretained(consumer))));
  AddRequest(request, consumer->cancelable_consumer());
  ScheduleTask(base::Bind(func, this, request));
  return request->handle();
}

template<typename BackendFunc, typename ArgA>
CancelableRequestProvider::Handle PasswordStore::Schedule(
    BackendFunc func, PasswordStoreConsumer* consumer, const ArgA& a) {
  scoped_refptr<GetLoginsRequest> request(NewGetLoginsRequest(
      base::Bind(&PasswordStoreConsumer::OnPasswordStoreRequestDone,
                 base::Unretained(consumer))));
  AddRequest(request, consumer->cancelable_consumer());
  ScheduleTask(base::Bind(func, this, request, a));
  return request->handle();
}

void PasswordStore::WrapModificationTask(base::Closure task) {
#if !defined(OS_MACOSX)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
#endif  // !defined(OS_MACOSX)
  task.Run();
  PostNotifyLoginsChanged();
}

void PasswordStore::PostNotifyLoginsChanged() {
#if !defined(OS_MACOSX)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
#endif  // !defined(OS_MACOSX)

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PasswordStore::NotifyLoginsChanged, this));
}

void PasswordStore::NotifyLoginsChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(Observer, observers_, OnLoginsChanged());
}
