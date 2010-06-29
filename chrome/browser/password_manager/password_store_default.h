// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT_H_

#include <map>

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/password_manager/login_database.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/webdata/web_data_service.h"

// Simple password store implementation that delegates everything to
// the LoginDatabase.
class PasswordStoreDefault : public PasswordStore,
                             public WebDataServiceConsumer {
 public:
  // Takes ownership of |login_db|.
  PasswordStoreDefault(LoginDatabase* login_db,
                       Profile* profile,
                       WebDataService* web_data_service);

 protected:
  virtual ~PasswordStoreDefault();

  // Implements PasswordStore interface.
  void ReportMetricsImpl();
  void AddLoginImpl(const webkit_glue::PasswordForm& form);
  void UpdateLoginImpl(const webkit_glue::PasswordForm& form);
  void RemoveLoginImpl(const webkit_glue::PasswordForm& form);
  void RemoveLoginsCreatedBetweenImpl(const base::Time& delete_begin,
                                      const base::Time& delete_end);
  void GetLoginsImpl(GetLoginsRequest* request,
                     const webkit_glue::PasswordForm& form);
  void GetAutofillableLoginsImpl(GetLoginsRequest* request);
  void GetBlacklistLoginsImpl(GetLoginsRequest* request);
  bool FillAutofillableLogins(
      std::vector<webkit_glue::PasswordForm*>* forms);
  bool FillBlacklistLogins(
      std::vector<webkit_glue::PasswordForm*>* forms);

  scoped_refptr<WebDataService> web_data_service_;

  // Implements the WebDataService consumer interface.
  void OnWebDataServiceRequestDone(WebDataService::Handle handle,
                                   const WDTypedResult *result);

 protected:
  inline bool DeleteAndRecreateDatabaseFile() {
    return login_db_->DeleteAndRecreateDatabaseFile();
  }

 private:
  // Migrates logins from the WDS to the LoginDatabase.
  void MigrateIfNecessary();

  scoped_ptr<LoginDatabase> login_db_;
  Profile* profile_;

  std::set<WebDataService::Handle> handles_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreDefault);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT_H_
