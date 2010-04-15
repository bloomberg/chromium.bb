// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_LINUX_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_LINUX_H_

#include <map>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/password_manager/login_database.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/webdata/web_data_service.h"

class Task;

// Simple password store implementation that delegates everything to
// the LoginDatabase.
class PasswordStoreLinux : public PasswordStore,
                           public WebDataServiceConsumer {
 public:
  // Takes ownership of |login_db|.
  PasswordStoreLinux(LoginDatabase* login_db,
                     Profile* profile,
                     WebDataService* web_data_service);

 protected:
  virtual ~PasswordStoreLinux();

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

 private:
  // Migrates logins from the WDS to the LoginDatabase.
  void MigrateIfNecessary();

  // Implements the WebDataService consumer interface.
  void OnWebDataServiceRequestDone(WebDataService::Handle handle,
                                   const WDTypedResult *result);

  scoped_ptr<LoginDatabase> login_db_;
  Profile* profile_;
  scoped_refptr<WebDataService> web_data_service_;

  std::set<WebDataService::Handle> handles_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreLinux);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_LINUX_H_
