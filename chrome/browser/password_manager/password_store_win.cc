// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_win.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/ie7_password.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/profile.h"

using std::map;
using std::vector;
using webkit_glue::PasswordForm;

PasswordStoreWin::PasswordStoreWin(LoginDatabase* login_database,
                                   Profile* profile,
                                   WebDataService* web_data_service)
    : PasswordStoreDefault(login_database, profile, web_data_service) {
}

PasswordStoreWin::~PasswordStoreWin() {
  for (PendingRequestMap::const_iterator it = pending_requests_.begin();
       it != pending_requests_.end(); ++it) {
    web_data_service_->CancelRequest(it->first);
  }
}

int PasswordStoreWin::GetLogins(const webkit_glue::PasswordForm& form,
                                PasswordStoreConsumer* consumer) {
  int request_handle = PasswordStoreDefault::GetLogins(form, consumer);
  pending_request_forms_.insert(PendingRequestFormMap::value_type(
      request_handle, form));
  return request_handle;
}

void PasswordStoreWin::NotifyConsumer(GetLoginsRequest* request,
                                      const std::vector<PasswordForm*> forms) {
  if (!forms.empty()) {
    pending_request_forms_.erase(request->handle);
    PasswordStore::NotifyConsumer(request, forms);
  } else {
    PendingRequestFormMap::iterator it(pending_request_forms_.find(
        request->handle));
    if (it != pending_request_forms_.end()) {
      IE7PasswordInfo info;
      std::wstring url = ASCIIToWide(it->second.origin.spec());
      info.url_hash = ie7_password::GetUrlHash(url);
      WebDataService::Handle handle = web_data_service_->GetIE7Login(info,
                                                                     this);
      TrackRequest(handle, request);
    }
  }
}

void PasswordStoreWin::OnWebDataServiceRequestDone(
    WebDataService::Handle handle, const WDTypedResult* result) {
  if (!result)
    return;  // The WDS returns NULL if it is shutting down.

  if (PASSWORD_IE7_RESULT == result->GetType()) {
    scoped_ptr<GetLoginsRequest> request(TakeRequestWithHandle(handle));

    // If the request was cancelled, we are done.
    if (!request.get())
      return;

    // This is a response from WebDataService::GetIE7Login.
    PendingRequestFormMap::iterator it(pending_request_forms_.find(
        request->handle));
    DCHECK(pending_request_forms_.end() != it);
    PasswordForm* ie7_form = GetIE7Result(result, it->second);

    std::vector<PasswordForm*> forms;
    if (ie7_form)
      forms.push_back(ie7_form);

    pending_request_forms_.erase(it);
    PasswordStore::NotifyConsumer(request.release(), forms);
  } else {
    PasswordStoreDefault::OnWebDataServiceRequestDone(handle, result);
  }
}

void PasswordStoreWin::TrackRequest(WebDataService::Handle handle,
                                    GetLoginsRequest* request) {
  pending_requests_.insert(PendingRequestMap::value_type(handle, request));
}

PasswordStoreDefault::GetLoginsRequest*
    PasswordStoreWin::TakeRequestWithHandle(WebDataService::Handle handle) {
  PendingRequestMap::iterator it(pending_requests_.find(handle));
  if (it == pending_requests_.end())
    return NULL;

  GetLoginsRequest* request = it->second;
  pending_requests_.erase(it);
  return request;
}

PasswordForm* PasswordStoreWin::GetIE7Result(const WDTypedResult *result,
                                             const PasswordForm& form) {
  const WDResult<IE7PasswordInfo>* r =
      static_cast<const WDResult<IE7PasswordInfo>*>(result);
  IE7PasswordInfo info = r->GetValue();

  if (!info.encrypted_data.empty()) {
    // We got a result.
    // Delete the entry. If it's good we will add it to the real saved password
    // table.
    web_data_service_->RemoveIE7Login(info);
    std::wstring username;
    std::wstring password;
    std::wstring url = ASCIIToWide(form.origin.spec());
    if (!ie7_password::DecryptPassword(url, info.encrypted_data,
                                       &username, &password)) {
      return NULL;
    }

    PasswordForm* auto_fill = new PasswordForm(form);
    auto_fill->username_value = username;
    auto_fill->password_value = password;
    auto_fill->preferred = true;
    auto_fill->ssl_valid = form.origin.SchemeIsSecure();
    auto_fill->date_created = info.date_created;
    // Add this PasswordForm to the saved password table.
    AddLogin(*auto_fill);
    return auto_fill;
  }
  return NULL;
}
