// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_win.h"

#include <map>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/password_manager/ie7_password.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webdata/web_data_service.h"

using content::BrowserThread;
using content::PasswordForm;

// Handles requests to WebDataService.
class PasswordStoreWin::DBHandler : public WebDataServiceConsumer {
 public:
  DBHandler(WebDataService* web_data_service,
            PasswordStoreWin* password_store)
      : web_data_service_(web_data_service),
        password_store_(password_store) {
  }

  ~DBHandler();

  // Requests the IE7 login for |form|. This is async. |callback_runner| will be
  // run when complete.
  void GetIE7Login(
      const PasswordForm& form,
      const PasswordStoreWin::ConsumerCallbackRunner& callback_runner);

 private:
  struct RequestInfo {
    RequestInfo() {}

    RequestInfo(PasswordForm* request_form,
                const PasswordStoreWin::ConsumerCallbackRunner& runner)
        : form(request_form),
          callback_runner(runner) {}

    PasswordForm* form;
    PasswordStoreWin::ConsumerCallbackRunner callback_runner;
  };

  // Holds info associated with in-flight GetIE7Login requests.
  typedef std::map<WebDataService::Handle, RequestInfo> PendingRequestMap;

  // Gets logins from IE7 if no others are found. Also copies them into
  // Chrome's WebDatabase so we don't need to look next time.
  PasswordForm* GetIE7Result(const WDTypedResult* result,
                             const PasswordForm& form);

  // WebDataServiceConsumer implementation.
  virtual void OnWebDataServiceRequestDone(
      WebDataService::Handle handle,
      const WDTypedResult* result) OVERRIDE;

  scoped_refptr<WebDataService> web_data_service_;

  // This creates a cycle between us and PasswordStore. The cycle is broken
  // from PasswordStoreWin::Shutdown, which deletes us.
  scoped_refptr<PasswordStoreWin> password_store_;

  PendingRequestMap pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(DBHandler);
};

PasswordStoreWin::DBHandler::~DBHandler() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  for (PendingRequestMap::const_iterator i = pending_requests_.begin();
       i != pending_requests_.end();
       ++i) {
    web_data_service_->CancelRequest(i->first);
    delete i->second.form;
  }
}

void PasswordStoreWin::DBHandler::GetIE7Login(
    const PasswordForm& form,
    const PasswordStoreWin::ConsumerCallbackRunner& callback_runner) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  IE7PasswordInfo info;
  info.url_hash = ie7_password::GetUrlHash(UTF8ToWide(form.origin.spec()));
  WebDataService::Handle handle = web_data_service_->GetIE7Login(info, this);
  pending_requests_[handle] =
      RequestInfo(new PasswordForm(form), callback_runner);
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

  PendingRequestMap::iterator i = pending_requests_.find(handle);
  DCHECK(i != pending_requests_.end());

  scoped_ptr<PasswordForm> form(i->second.form);
  PasswordStoreWin::ConsumerCallbackRunner callback_runner(
      i->second.callback_runner);
  pending_requests_.erase(i);

  std::vector<content::PasswordForm*> matched_forms;

  if (!result) {
    // The WDS returns NULL if it is shutting down. Run callback with empty
    // result.
    callback_runner.Run(matched_forms);
    return;
  }

  DCHECK_EQ(PASSWORD_IE7_RESULT, result->GetType());
  PasswordForm* ie7_form = GetIE7Result(result, *form);

  if (ie7_form)
    matched_forms.push_back(ie7_form);

  callback_runner.Run(matched_forms);
}

PasswordStoreWin::PasswordStoreWin(LoginDatabase* login_database,
                                   Profile* profile,
                                   WebDataService* web_data_service)
    : PasswordStoreDefault(login_database, profile) {
  db_handler_.reset(new DBHandler(web_data_service, this));
}

PasswordStoreWin::~PasswordStoreWin() {
}

void PasswordStoreWin::ShutdownOnDBThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  db_handler_.reset();
}

void PasswordStoreWin::ShutdownOnUIThread() {
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&PasswordStoreWin::ShutdownOnDBThread, this));
  PasswordStoreDefault::ShutdownOnUIThread();
}

void PasswordStoreWin::GetIE7LoginIfNecessary(
    const PasswordForm& form,
    const ConsumerCallbackRunner& callback_runner,
    const std::vector<content::PasswordForm*>& matched_forms) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::DB));
  if (matched_forms.empty() && db_handler_.get()) {
    db_handler_->GetIE7Login(form, callback_runner);
  } else {
    // No need to get IE7 login.
    callback_runner.Run(matched_forms);
  }
}

void PasswordStoreWin::GetLoginsImpl(
    const PasswordForm& form,
    const ConsumerCallbackRunner& callback_runner) {
  ConsumerCallbackRunner get_ie7_login =
      base::Bind(&PasswordStoreWin::GetIE7LoginIfNecessary,
                 this, form, callback_runner);
  PasswordStoreDefault::GetLoginsImpl(form, get_ie7_login);
}
