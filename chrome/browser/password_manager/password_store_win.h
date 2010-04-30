// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN_H_

#include <map>
#include <vector>

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/password_manager/password_store_default.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_database.h"

// Windows PasswordStore implementation that uses the default implementation,
// but also uses IE7 passwords if no others found.
class PasswordStoreWin : public PasswordStoreDefault {
 public:
  // FilePath specifies path to WebDatabase.
  PasswordStoreWin(LoginDatabase* login_database,
                   Profile* profile,
                   WebDataService* web_data_service);

  // Overridden so that we can save the form for later use.
  virtual int GetLogins(const webkit_glue::PasswordForm& form,
                        PasswordStoreConsumer* consumer);

 private:
  virtual ~PasswordStoreWin();

  // See PasswordStoreDefault.
  void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                   const WDTypedResult* result);

  virtual void NotifyConsumer(
      GetLoginsRequest* request,
      const std::vector<webkit_glue::PasswordForm*> forms);

  // Takes ownership of |request| and tracks it under |handle|.
  void TrackRequest(WebDataService::Handle handle, GetLoginsRequest* request);

  // Finds the GetLoginsRequest associated with the in-flight WebDataService
  // request identified by |handle|, removes it from the tracking list, and
  // returns it. Ownership of the GetLoginsRequest passes to the caller.
  // Returns NULL if the request has been cancelled.
  GetLoginsRequest* TakeRequestWithHandle(WebDataService::Handle handle);

  // Gets logins from IE7 if no others are found. Also copies them into
  // Chrome's WebDatabase so we don't need to look next time.
  webkit_glue::PasswordForm* GetIE7Result(
      const WDTypedResult* result,
      const webkit_glue::PasswordForm& form);

  // Holds requests associated with in-flight GetLogin queries.
  typedef std::map<int, GetLoginsRequest*> PendingRequestMap;
  PendingRequestMap pending_requests_;

  // Holds forms associated with in-flight GetLogin queries.
  typedef std::map<int, webkit_glue::PasswordForm> PendingRequestFormMap;
  PendingRequestFormMap pending_request_forms_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreWin);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN_H_
