// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN

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
  explicit PasswordStoreWin(WebDataService* web_data_service);

  // Overridden so that we can save the form for later use.
  virtual int GetLogins(const webkit_glue::PasswordForm& form,
                        PasswordStoreConsumer* consumer);
  virtual void CancelLoginsQuery(int handle);

 private:
  virtual ~PasswordStoreWin() {}

  // See PasswordStoreDefault.
  void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                   const WDTypedResult* result);

  // Removes the form for |handle| from pending_request_forms_ (if any).
  void DeleteFormForRequest(int handle);

  // Cleans up internal state related to |request|, and sends its results to
  // the request's consumer.
  void CompleteRequest(GetLoginsRequest* request,
                       const std::vector<webkit_glue::PasswordForm*>& forms);

  // Gets logins from IE7 if no others are found. Also copies them into
  // Chrome's WebDatabase so we don't need to look next time.
  webkit_glue::PasswordForm* GetIE7Result(
      const WDTypedResult* result,
      const webkit_glue::PasswordForm& form);

  // Holds forms associated with in-flight GetLogin queries.
  typedef std::map<int, webkit_glue::PasswordForm> PendingRequestFormMap;
  PendingRequestFormMap pending_request_forms_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreWin);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_WIN
