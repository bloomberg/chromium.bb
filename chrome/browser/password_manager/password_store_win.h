// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN_H_
#pragma once

#include <map>

#include "chrome/browser/password_manager/password_store_default.h"

class LoginDatabase;
class Profile;
class WebDataService;

namespace webkit_glue {
struct PasswordForm;
}

// Windows PasswordStore implementation that uses the default implementation,
// but also uses IE7 passwords if no others found.
class PasswordStoreWin : public PasswordStoreDefault {
 public:
  // FilePath specifies path to WebDatabase.
  PasswordStoreWin(LoginDatabase* login_database,
                   Profile* profile,
                   WebDataService* web_data_service);

 private:
  virtual ~PasswordStoreWin();

  virtual GetLoginsRequest* NewGetLoginsRequest(
      GetLoginsCallback* callback) OVERRIDE;

  // See PasswordStoreDefault.
  virtual void ForwardLoginsResult(GetLoginsRequest* request) OVERRIDE;
  virtual void OnWebDataServiceRequestDone(
      WebDataService::Handle h, const WDTypedResult* result) OVERRIDE;

  // Overridden so that we can save the form for later use.
  virtual void GetLoginsImpl(GetLoginsRequest* request,
                             const webkit_glue::PasswordForm& form) OVERRIDE;

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
  typedef std::map<WebDataService::Handle, GetLoginsRequest*> PendingRequestMap;
  PendingRequestMap pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreWin);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN_H_
