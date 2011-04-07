// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_win.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/ie7_password.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/profiles/profile.h"

using webkit_glue::PasswordForm;

namespace {
// Subclass GetLoginsRequest in order to hold a copy of the form information
// from the GetLogins request for the ForwardLoginsResult call. Note that the
// other calls such as GetBlacklistLogins and GetAutofillableLogins, the form is
// not set.
class FormGetLoginsRequest : public PasswordStore::GetLoginsRequest {
 public:
  explicit FormGetLoginsRequest(PasswordStore::GetLoginsCallback* callback)
      : GetLoginsRequest(callback) {}

  // We hold a copy of the |form| used in GetLoginsImpl as a pointer.  If the
  // form is not set (is NULL), then we are not a GetLogins request.
  void SetLoginsRequestForm(const PasswordForm& form) {
    form_.reset(new PasswordForm(form));
  }
  PasswordForm* form() const {
    return form_.get();
  }
  bool IsLoginsRequest() const { return !!form_.get(); }

 private:
  scoped_ptr<PasswordForm> form_;
};
}

PasswordStoreWin::PasswordStoreWin(LoginDatabase* login_database,
                                   Profile* profile,
                                   WebDataService* web_data_service)
    : PasswordStoreDefault(login_database, profile, web_data_service) {
}

PasswordStoreWin::~PasswordStoreWin() {
  DCHECK(pending_requests_.empty() ||
         BrowserThread::CurrentlyOn(BrowserThread::DB));

  for (PendingRequestMap::const_iterator it = pending_requests_.begin();
       it != pending_requests_.end(); ++it) {
    web_data_service_->CancelRequest(it->first);
  }
}

PasswordStore::GetLoginsRequest* PasswordStoreWin::NewGetLoginsRequest(
    GetLoginsCallback* callback) {
  return new FormGetLoginsRequest(callback);
}

void PasswordStoreWin::ForwardLoginsResult(GetLoginsRequest* request) {
  if (static_cast<FormGetLoginsRequest*>(request)->IsLoginsRequest() &&
      request->value.empty()) {
    IE7PasswordInfo info;
    std::wstring url = ASCIIToWide(
        static_cast<FormGetLoginsRequest*>(request)->form()->origin.spec());
    info.url_hash = ie7_password::GetUrlHash(url);
    WebDataService::Handle handle = web_data_service_->GetIE7Login(info,
                                                                   this);
    TrackRequest(handle, request);
  } else {
    PasswordStore::ForwardLoginsResult(request);
  }
}

void PasswordStoreWin::OnWebDataServiceRequestDone(
    WebDataService::Handle handle, const WDTypedResult* result) {
  if (!result)
    return;  // The WDS returns NULL if it is shutting down.

  if (PASSWORD_IE7_RESULT == result->GetType()) {
    scoped_refptr<GetLoginsRequest> request(TakeRequestWithHandle(handle));

    // If the request was cancelled, we are done.
    if (!request.get())
      return;

    // This is a response from WebDataService::GetIE7Login.
    PasswordForm* form = static_cast<FormGetLoginsRequest*>(
        request.get())->form();
    DCHECK(form);
    PasswordForm* ie7_form = GetIE7Result(result, *form);

    if (ie7_form)
      request->value.push_back(ie7_form);

    PasswordStore::ForwardLoginsResult(request.get());
  } else {
    PasswordStoreDefault::OnWebDataServiceRequestDone(handle, result);
  }
}

void PasswordStoreWin::GetLoginsImpl(GetLoginsRequest* request,
                                     const PasswordForm& form) {
  static_cast<FormGetLoginsRequest*>(request)->SetLoginsRequestForm(form);

  PasswordStoreDefault::GetLoginsImpl(request, form);
}

void PasswordStoreWin::TrackRequest(WebDataService::Handle handle,
                                    GetLoginsRequest* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  pending_requests_.insert(PendingRequestMap::value_type(handle, request));
}

PasswordStoreDefault::GetLoginsRequest* PasswordStoreWin::TakeRequestWithHandle(
    WebDataService::Handle handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

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

    PasswordForm* autofill = new PasswordForm(form);
    autofill->username_value = username;
    autofill->password_value = password;
    autofill->preferred = true;
    autofill->ssl_valid = form.origin.SchemeIsSecure();
    autofill->date_created = info.date_created;
    // Add this PasswordForm to the saved password table.
    AddLogin(*autofill);
    return autofill;
  }
  return NULL;
}
