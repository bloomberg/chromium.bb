// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_win.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/password_manager/ie7_password.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/profile.h"

using std::map;
using std::vector;
using webkit_glue::PasswordForm;

PasswordStoreWin::PasswordStoreWin(WebDataService* web_data_service)
    : PasswordStoreDefault(web_data_service) {
}

void PasswordStoreWin::CancelLoginsQuery(int handle) {
  PasswordStoreDefault::CancelLoginsQuery(handle);
  DeleteFormForRequest(handle);
}

int PasswordStoreWin::GetLogins(const webkit_glue::PasswordForm& form,
                                PasswordStoreConsumer* consumer) {
  int request_handle = PasswordStoreDefault::GetLogins(form, consumer);
  pending_request_forms_.insert(PendingRequestFormMap::value_type(
      request_handle, form));
  return request_handle;
}

void PasswordStoreWin::OnWebDataServiceRequestDone(
    WebDataService::Handle h, const WDTypedResult *result) {
  scoped_ptr<GetLoginsRequest> request(TakeRequestWithHandle(h));
  // If the request was cancelled, we are done.
  if (!request.get())
    return;

  DCHECK(result);
  if (!result)
    return;

  switch (result->GetType()) {
    case PASSWORD_RESULT: {
      // This is a response from a WebDataService::Get*Logins method.
      const WDResult<std::vector<PasswordForm*> >* r =
        static_cast<const WDResult<std::vector<PasswordForm*> >*>(result);
      std::vector<PasswordForm*> result_value = r->GetValue();

      if (result_value.size()) {
        // If we found some results then return them now.
        CompleteRequest(request.get(), result_value);
        return;
      } else {
        // Otherwise try finding IE7 logins if we were looking for a specific
        // page.
        PendingRequestFormMap::iterator it(pending_request_forms_.find(
            request->handle));
        if (it != pending_request_forms_.end()) {
          IE7PasswordInfo info;
          std::wstring url = ASCIIToWide(it->second.origin.spec());
          info.url_hash = ie7_password::GetUrlHash(url);

          if (web_data_service_->IsRunning()) {
            WebDataService::Handle web_data_handle =
                web_data_service_->GetIE7Login(info, this);
            TrackRequest(web_data_handle, request.release());
          }
        }
      }
      break;
    }

    case PASSWORD_IE7_RESULT: {
      // This is a response from WebDataService::GetIE7Login.
      PendingRequestFormMap::iterator it(pending_request_forms_.find(
          request->handle));
      PasswordForm* ie7_form = GetIE7Result(result, it->second);

      std::vector<PasswordForm*> forms;
      if (ie7_form)
        forms.push_back(ie7_form);

      CompleteRequest(request.get(), forms);
      break;
    }
  }
}

void PasswordStoreWin::DeleteFormForRequest(int handle) {
  PendingRequestFormMap::iterator it(pending_request_forms_.find(handle));
  if (it != pending_request_forms_.end()) {
    pending_request_forms_.erase(it);
  }
}

void PasswordStoreWin::CompleteRequest(
    GetLoginsRequest* request,
    const std::vector<webkit_glue::PasswordForm*>& forms) {
  DeleteFormForRequest(request->handle);
  request->consumer->OnPasswordStoreRequestDone(request->handle, forms);
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
