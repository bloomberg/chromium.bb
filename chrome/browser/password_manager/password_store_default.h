// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT

#include <map>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/webdata/web_data_service.h"

class Task;

// Simple password store implementation that delegates everything to
// the WebDatabase.
// TODO(stuartmorgan): This is a temprorary shim for Windows from the new
// PasswordStore interface to the old model of storing passwords in the
// WebDatabase. This should be replaced by a self-contained Windows
// implementation once PasswordStore is completed.
class PasswordStoreDefault : public PasswordStore,
                             public WebDataServiceConsumer {
 public:
  explicit PasswordStoreDefault(WebDataService* web_data_service);

  // Overridden to bypass the threading logic in PasswordStore, since
  // WebDataService's API is not threadsafe.
  virtual void AddLogin(const webkit_glue::PasswordForm& form);
  virtual void UpdateLogin(const webkit_glue::PasswordForm& form);
  virtual void RemoveLogin(const webkit_glue::PasswordForm& form);
  virtual void RemoveLoginsCreatedBetween(const base::Time& delete_begin,
                                          const base::Time& delete_end);
  virtual int GetLogins(const webkit_glue::PasswordForm& form,
                        PasswordStoreConsumer* consumer);
  virtual int GetAutofillableLogins(PasswordStoreConsumer* consumer);
  virtual int GetBlacklistLogins(PasswordStoreConsumer* consumer);
  virtual void CancelLoginsQuery(int handle);

 protected:
  virtual ~PasswordStoreDefault();

  // Implements PasswordStore interface.
  void AddLoginImpl(const webkit_glue::PasswordForm& form);
  void UpdateLoginImpl(const webkit_glue::PasswordForm& form);
  void RemoveLoginImpl(const webkit_glue::PasswordForm& form);
  void RemoveLoginsCreatedBetweenImpl(const base::Time& delete_begin,
                                      const base::Time& delete_end);
  void GetLoginsImpl(GetLoginsRequest* request,
                     const webkit_glue::PasswordForm& form);
  void GetAutofillableLoginsImpl(GetLoginsRequest* request);
  void GetBlacklistLoginsImpl(GetLoginsRequest* request);

  // Called when a WebDataService method finishes.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

  // Finds the GetLoginsRequest associated with the in-flight WebDataService
  // request identified by |handle|, removes it from the tracking list, and
  // returns it. Ownership of the GetLoginsRequest passes to the caller.
  // Returns NULL if the request has been cancelled.
  GetLoginsRequest* TakeRequestWithHandle(WebDataService::Handle handle);

  // Takes ownership of |request| and tracks it under |handle|.
  void TrackRequest(WebDataService::Handle handle, GetLoginsRequest* request);

  scoped_refptr<WebDataService> web_data_service_;

 private:
  // Creates a new request, stored in pending_requests_, for the given
  // WebDataService query, and returns the request handle that should be used
  // for it in the client API.
  int CreateNewRequestForQuery(WebDataService::Handle web_data_handle,
                               PasswordStoreConsumer* consumer);

  // Next handle to return from our Get*Logins() overrides to allow callers to
  // track their request.
  int handle_;

  // Methods in this class call async WebDataService methods.  This mapping
  // remembers which WebDataService request corresponds to which PasswordStore
  // request.
  typedef std::map<WebDataService::Handle, GetLoginsRequest*> PendingRequestMap;
  PendingRequestMap pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreDefault);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT
