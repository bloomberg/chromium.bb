// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_default.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_constants.h"

#include "base/logging.h"
#include "base/task.h"

using webkit_glue::PasswordForm;

PasswordStoreDefault::PasswordStoreDefault(WebDataService* web_data_service)
    : web_data_service_(web_data_service), handle_(0) {
}

PasswordStoreDefault::~PasswordStoreDefault() {
  for (PendingRequestMap::const_iterator it = pending_requests_.begin();
       it != pending_requests_.end(); ++it) {
    web_data_service_->CancelRequest(it->first);
  }
}

// Override all the public methods to avoid passthroughs to the Impl
// versions. Since we are calling through to WebDataService, which is
// asynchronous, we'll still behave as the caller expects.
void PasswordStoreDefault::AddLogin(const PasswordForm& form) {
  web_data_service_->AddLogin(form);
}

void PasswordStoreDefault::UpdateLogin(const PasswordForm& form) {
  web_data_service_->UpdateLogin(form);
}

void PasswordStoreDefault::RemoveLogin(const PasswordForm& form) {
  web_data_service_->RemoveLogin(form);
}

void PasswordStoreDefault::RemoveLoginsCreatedBetween(
    const base::Time& delete_begin, const base::Time& delete_end) {
  web_data_service_->RemoveLoginsCreatedBetween(delete_begin, delete_end);
}

int PasswordStoreDefault::GetLogins(const PasswordForm& form,
                                    PasswordStoreConsumer* consumer) {
  WebDataService::Handle web_data_handle = web_data_service_->GetLogins(form,
                                                                        this);
  return CreateNewRequestForQuery(web_data_handle, consumer);
}

int PasswordStoreDefault::GetAutofillableLogins(
    PasswordStoreConsumer* consumer) {
  WebDataService::Handle web_data_handle =
      web_data_service_->GetAutofillableLogins(this);
  return CreateNewRequestForQuery(web_data_handle, consumer);
}

int PasswordStoreDefault::GetBlacklistLogins(
    PasswordStoreConsumer* consumer) {
  WebDataService::Handle web_data_handle =
      web_data_service_->GetBlacklistLogins(this);
  return CreateNewRequestForQuery(web_data_handle, consumer);
}

void PasswordStoreDefault::CancelLoginsQuery(int handle) {
  for (PendingRequestMap::iterator it = pending_requests_.begin();
       it != pending_requests_.end(); ++it) {
    GetLoginsRequest* request = it->second;
    if (request->handle == handle) {
      web_data_service_->CancelRequest(it->first);
      delete request;
      pending_requests_.erase(it);
      return;
    }
  }
}

void PasswordStoreDefault::AddLoginImpl(const PasswordForm& form) {
  NOTREACHED();
}

void PasswordStoreDefault::RemoveLoginImpl(const PasswordForm& form) {
  NOTREACHED();
}

void PasswordStoreDefault::RemoveLoginsCreatedBetweenImpl(
    const base::Time& delete_begin, const base::Time& delete_end) {
  NOTREACHED();
}

void PasswordStoreDefault::UpdateLoginImpl(const PasswordForm& form) {
  NOTREACHED();
}

void PasswordStoreDefault::GetLoginsImpl(
    GetLoginsRequest* request, const webkit_glue::PasswordForm& form) {
  NOTREACHED();
}

void PasswordStoreDefault::GetAutofillableLoginsImpl(
    GetLoginsRequest* request) {
  NOTREACHED();
}

void PasswordStoreDefault::GetBlacklistLoginsImpl(
    GetLoginsRequest* request) {
  NOTREACHED();
}

void PasswordStoreDefault::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult *result) {
  scoped_ptr<GetLoginsRequest> request(TakeRequestWithHandle(h));
  // If the request was cancelled, we are done.
  if (!request.get())
    return;

  DCHECK(result);
  if (!result)
    return;

  const WDResult<std::vector<PasswordForm*> >* r =
      static_cast<const WDResult<std::vector<PasswordForm*> >*>(result);

  request->consumer->OnPasswordStoreRequestDone(request->handle,
                                                r->GetValue());
}

PasswordStoreDefault::GetLoginsRequest*
    PasswordStoreDefault::TakeRequestWithHandle(WebDataService::Handle handle) {
  PendingRequestMap::iterator it(pending_requests_.find(handle));
  if (it == pending_requests_.end())
    return NULL;

  GetLoginsRequest* request = it->second;
  pending_requests_.erase(it);
  return request;
}

int PasswordStoreDefault::CreateNewRequestForQuery(
    WebDataService::Handle web_data_handle, PasswordStoreConsumer* consumer) {
  int api_handle = handle_++;
  GetLoginsRequest* request = new GetLoginsRequest(consumer, api_handle);
  TrackRequest(web_data_handle, request);
  return api_handle;
}

void PasswordStoreDefault::TrackRequest(WebDataService::Handle handle,
                                        GetLoginsRequest* request) {
  pending_requests_.insert(PendingRequestMap::value_type(handle, request));
}
