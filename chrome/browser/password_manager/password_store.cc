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

void PasswordStore::GetLoginsRequest::ApplyIgnoreLoginsCutoff() {
  if (!ignore_logins_cutoff_.is_null()) {
    // Count down rather than up since we may be deleting elements.
    // Note that in principle it could be more efficient to copy the whole array
    // since that's worst-case linear time, but we expect that elements will be
    // deleted rarely and lists will be small, so this avoids the copies.
    for (size_t i = value.size(); i > 0; --i) {
      if (value[i - 1]->date_created < ignore_logins_cutoff_) {
        delete value[i - 1];
        value.erase(value.begin() + (i - 1));
      }
    }
  }
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
  // Per http://crbug.com/121738, we deliberately ignore saved logins for
  // http*://www.google.com/ that were stored prior to 2012. (Google now uses
  // https://accounts.google.com/ for all login forms, so these should be
  // unused.) We don't delete them just yet, and they'll still be visible in the
  // password manager, but we won't use them to autofill any forms. This is a
  // security feature to help minimize damage that can be done by XSS attacks.
  // TODO(mdm): actually delete them at some point, say M24 or so.
  base::Time ignore_logins_cutoff;  // the null time
  if (form.scheme == PasswordForm::SCHEME_HTML &&
      (form.signon_realm == "http://www.google.com" ||
       form.signon_realm == "http://www.google.com/" ||
       form.signon_realm == "https://www.google.com" ||
       form.signon_realm == "https://www.google.com/")) {
    static const base::Time::Exploded exploded_cutoff =
        { 2012, 1, 0, 1, 0, 0, 0, 0 };  // 00:00 Jan 1 2012
    ignore_logins_cutoff = base::Time::FromUTCExploded(exploded_cutoff);
  }
  return Schedule(&PasswordStore::GetLoginsImpl, consumer, form,
                  ignore_logins_cutoff);
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
  request->ApplyIgnoreLoginsCutoff();
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

template<typename BackendFunc>
CancelableRequestProvider::Handle PasswordStore::Schedule(
    BackendFunc func, PasswordStoreConsumer* consumer,
    const PasswordForm& form, const base::Time& ignore_logins_cutoff) {
  scoped_refptr<GetLoginsRequest> request(NewGetLoginsRequest(
      base::Bind(&PasswordStoreConsumer::OnPasswordStoreRequestDone,
                 base::Unretained(consumer))));
  request->set_ignore_logins_cutoff(ignore_logins_cutoff);
  AddRequest(request, consumer->cancelable_consumer());
  ScheduleTask(base::Bind(func, this, request, form));
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
