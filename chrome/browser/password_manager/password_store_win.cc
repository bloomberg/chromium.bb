// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_win.h"

#include <map>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/os_crypt/ie7_password_win.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/webdata/password_web_data_service_win.h"
#include "content/public/browser/browser_thread.h"

using autofill::PasswordForm;
using content::BrowserThread;
using password_manager::PasswordStoreDefault;

// Handles requests to PasswordWebDataService.
class PasswordStoreWin::DBHandler : public WebDataServiceConsumer {
 public:
  typedef base::Callback<void(ScopedVector<PasswordForm>)> ResultCallback;

  DBHandler(const scoped_refptr<PasswordWebDataService>& web_data_service,
            PasswordStoreWin* password_store)
      : web_data_service_(web_data_service), password_store_(password_store) {}

  ~DBHandler() override;

  // Requests the IE7 login for |form|. This is async. |result_callback| will be
  // run when complete.
  void GetIE7Login(const PasswordForm& form,
                   const ResultCallback& result_callback);

 private:
  struct RequestInfo {
    RequestInfo() {}

    RequestInfo(PasswordForm* request_form,
                const ResultCallback& result_callback)
        : form(request_form), result_callback(result_callback) {}

    PasswordForm* form;
    ResultCallback result_callback;
  };

  // Holds info associated with in-flight GetIE7Login requests.
  typedef std::map<PasswordWebDataService::Handle, RequestInfo>
      PendingRequestMap;

  // Gets logins from IE7 if no others are found. Also copies them into
  // Chrome's WebDatabase so we don't need to look next time.
  ScopedVector<autofill::PasswordForm> GetIE7Results(
      const WDTypedResult* result,
      const PasswordForm& form);

  // WebDataServiceConsumer implementation.
  void OnWebDataServiceRequestDone(
      PasswordWebDataService::Handle handle,
      const WDTypedResult* result) override;

  scoped_refptr<PasswordWebDataService> web_data_service_;

  // This creates a cycle between us and PasswordStore. The cycle is broken
  // from PasswordStoreWin::Shutdown, which deletes us.
  scoped_refptr<PasswordStoreWin> password_store_;

  PendingRequestMap pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(DBHandler);
};

PasswordStoreWin::DBHandler::~DBHandler() {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  for (PendingRequestMap::const_iterator i = pending_requests_.begin();
       i != pending_requests_.end();
       ++i) {
    web_data_service_->CancelRequest(i->first);
    delete i->second.form;
  }
}

void PasswordStoreWin::DBHandler::GetIE7Login(
    const PasswordForm& form,
    const ResultCallback& result_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  IE7PasswordInfo info;
  info.url_hash =
      ie7_password::GetUrlHash(base::UTF8ToWide(form.origin.spec()));
  PasswordWebDataService::Handle handle =
      web_data_service_->GetIE7Login(info, this);
  pending_requests_[handle] =
      RequestInfo(new PasswordForm(form), result_callback);
}

ScopedVector<autofill::PasswordForm> PasswordStoreWin::DBHandler::GetIE7Results(
    const WDTypedResult* result,
    const PasswordForm& form) {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  ScopedVector<autofill::PasswordForm> matched_forms;
  const WDResult<IE7PasswordInfo>* r =
      static_cast<const WDResult<IE7PasswordInfo>*>(result);
  IE7PasswordInfo info = r->GetValue();

  if (!info.encrypted_data.empty()) {
    // We got a result.
    // Delete the entry. If it's good we will add it to the real saved password
    // table.
    web_data_service_->RemoveIE7Login(info);
    std::vector<ie7_password::DecryptedCredentials> credentials;
    base::string16 url = base::ASCIIToUTF16(form.origin.spec());
    if (ie7_password::DecryptPasswords(url,
                                       info.encrypted_data,
                                       &credentials)) {
      for (size_t i = 0; i < credentials.size(); ++i) {
        PasswordForm* autofill = new PasswordForm();
        autofill->username_value = credentials[i].username;
        autofill->password_value = credentials[i].password;
        autofill->signon_realm = form.signon_realm;
        autofill->origin = form.origin;
        autofill->preferred = true;
        autofill->ssl_valid = form.origin.SchemeIsCryptographic();
        autofill->date_created = info.date_created;

        matched_forms.push_back(autofill);
        // Add this PasswordForm to the saved password table. We're on the DB
        // thread already, so we use AddLoginImpl.
        password_store_->AddLoginImpl(*autofill);
      }
    }
  }
  return matched_forms.Pass();
}

void PasswordStoreWin::DBHandler::OnWebDataServiceRequestDone(
    PasswordWebDataService::Handle handle,
    const WDTypedResult* result) {
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 PasswordStoreWin::DBHandler::OnWebDataServiceRequestDone"));

  DCHECK_CURRENTLY_ON(BrowserThread::DB);

  PendingRequestMap::iterator i = pending_requests_.find(handle);
  DCHECK(i != pending_requests_.end());

  scoped_ptr<PasswordForm> form(i->second.form);
  ResultCallback result_callback(i->second.result_callback);
  pending_requests_.erase(i);

  if (!result) {
    // The WDS returns NULL if it is shutting down. Run callback with empty
    // result.
    result_callback.Run(ScopedVector<autofill::PasswordForm>());
    return;
  }

  DCHECK_EQ(PASSWORD_IE7_RESULT, result->GetType());
  result_callback.Run(GetIE7Results(result, *form));
}

PasswordStoreWin::PasswordStoreWin(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner,
    scoped_ptr<password_manager::LoginDatabase> login_db,
    const scoped_refptr<PasswordWebDataService>& web_data_service)
    : PasswordStoreDefault(main_thread_runner,
                           db_thread_runner,
                           login_db.Pass()) {
  db_handler_.reset(new DBHandler(web_data_service, this));
}

PasswordStoreWin::~PasswordStoreWin() {
}

void PasswordStoreWin::ShutdownOnDBThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::DB);
  db_handler_.reset();
}

void PasswordStoreWin::Shutdown() {
  BrowserThread::PostTask(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&PasswordStoreWin::ShutdownOnDBThread, this));
  PasswordStoreDefault::Shutdown();
}

void PasswordStoreWin::GetLoginsImpl(const PasswordForm& form,
                                     AuthorizationPromptPolicy prompt_policy,
                                     scoped_ptr<GetLoginsRequest> request) {
  // When importing from IE7, the credentials are first stored into a temporary
  // Web SQL database. Then, after each GetLogins() request that does not yield
  // any matches from the LoginDatabase, the matching credentials in the Web SQL
  // database, if any, are returned as results instead, and simultaneously get
  // moved to the LoginDatabase, so next time they will be found immediately.
  // TODO(engedy): Make the IE7-specific code synchronous, so FillMatchingLogins
  // can be overridden instead. See: https://crbug.com/78830.
  // TODO(engedy): Credentials should be imported into the LoginDatabase in the
  // first place. See: https://crbug.com/456119.
  ScopedVector<autofill::PasswordForm> matched_forms(
      FillMatchingLogins(form, prompt_policy));
  if (matched_forms.empty() && db_handler_) {
    db_handler_->GetIE7Login(
        form, base::Bind(&GetLoginsRequest::NotifyConsumerWithResults,
                         base::Owned(request.release())));
  } else {
    request->NotifyConsumerWithResults(matched_forms.Pass());
  }
}
