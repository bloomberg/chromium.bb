// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_win.h"

#include <map>

#include "base/bind.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/ie7_password.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webdata/web_data_service.h"

using content::BrowserThread;
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

}  // namespace

// Handles requests to WebDataService.
class PasswordStoreWin::DBHandler : public WebDataServiceConsumer {
 public:
  DBHandler(WebDataService* web_data_service,
            PasswordStoreWin* password_store)
      : web_data_service_(web_data_service),
        password_store_(password_store) {
  }

  ~DBHandler();

  // Requests the IE7 login for |url| and |request|. This is async. The request
  // is processed when complete.
  void GetIE7Login(const GURL& url, GetLoginsRequest* request);

 private:
  // Holds requests associated with in-flight GetLogin queries.
  typedef std::map<WebDataService::Handle,
                   scoped_refptr<GetLoginsRequest> > PendingRequestMap;

  // Gets logins from IE7 if no others are found. Also copies them into
  // Chrome's WebDatabase so we don't need to look next time.
  PasswordForm* GetIE7Result(const WDTypedResult* result,
                             const PasswordForm& form);

  // WebDataServiceConsumer.
  virtual void OnWebDataServiceRequestDone(
      WebDataService::Handle handle,
      const WDTypedResult* result) OVERRIDE;

  scoped_refptr<WebDataService> web_data_service_;

  // This creates a cycle between us and PasswordStore. The cycle is broken
  // from PasswordStoreWin::Shutdown, which deletes us.
  scoped_refptr<PasswordStoreWin> password_store_;

  // Holds requests associated with in-flight GetLogin queries.
  PendingRequestMap pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(DBHandler);
};

PasswordStoreWin::DBHandler::~DBHandler() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  for (PendingRequestMap::const_iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    web_data_service_->CancelRequest(i->first);
  }
}

void PasswordStoreWin::DBHandler::GetIE7Login(const GURL& url,
                                              GetLoginsRequest* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  IE7PasswordInfo info;
  info.url_hash = ie7_password::GetUrlHash(UTF8ToWide(url.spec()));
  WebDataService::Handle handle = web_data_service_->GetIE7Login(info, this);
  pending_requests_.insert(PendingRequestMap::value_type(handle, request));
}

PasswordForm* PasswordStoreWin::DBHandler::GetIE7Result(
    const WDTypedResult *result,
    const PasswordForm& form) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

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
    // Add this PasswordForm to the saved password table. We're on the DB thread
    // already, so we use AddLoginImpl.
    password_store_->AddLoginImpl(*autofill);
    return autofill;
  }
  return NULL;
}

void PasswordStoreWin::DBHandler::OnWebDataServiceRequestDone(
    WebDataService::Handle handle,
    const WDTypedResult* result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));

  PendingRequestMap::iterator i(pending_requests_.find(handle));
  DCHECK(i != pending_requests_.end());
  scoped_refptr<GetLoginsRequest> request(i->second);
  pending_requests_.erase(i);

  if (!result)
    return;  // The WDS returns NULL if it is shutting down.

  DCHECK_EQ(PASSWORD_IE7_RESULT, result->GetType());
  PasswordForm* form =
      static_cast<FormGetLoginsRequest*>(request.get())->form();
  DCHECK(form);
  PasswordForm* ie7_form = GetIE7Result(result, *form);

  if (ie7_form)
    request->value.push_back(ie7_form);

  request->ForwardResult(GetLoginsRequest::TupleType(request->handle(),
                                                     request->value));
}

PasswordStoreWin::PasswordStoreWin(LoginDatabase* login_database,
                                   Profile* profile,
                                   WebDataService* web_data_service)
    : PasswordStoreDefault(login_database, profile, web_data_service) {
  db_handler_.reset(new DBHandler(web_data_service, this));
}

PasswordStoreWin::~PasswordStoreWin() {
}

void PasswordStoreWin::ShutdownOnDBThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  db_handler_.reset();
}

PasswordStore::GetLoginsRequest* PasswordStoreWin::NewGetLoginsRequest(
    GetLoginsCallback* callback) {
  return new FormGetLoginsRequest(callback);
}

void PasswordStoreWin::Shutdown() {
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&PasswordStoreWin::ShutdownOnDBThread, this));
  PasswordStoreDefault::Shutdown();
}

void PasswordStoreWin::ForwardLoginsResult(GetLoginsRequest* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (static_cast<FormGetLoginsRequest*>(request)->IsLoginsRequest() &&
      request->value.empty() && db_handler_.get()) {
    db_handler_->GetIE7Login(
        static_cast<FormGetLoginsRequest*>(request)->form()->origin,
        request);
  } else {
    PasswordStore::ForwardLoginsResult(request);
  }
}

void PasswordStoreWin::GetLoginsImpl(GetLoginsRequest* request,
                                     const PasswordForm& form) {
  static_cast<FormGetLoginsRequest*>(request)->SetLoginsRequestForm(form);

  PasswordStoreDefault::GetLoginsImpl(request, form);
}
