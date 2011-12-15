// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT_H_
#pragma once

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/password_manager/login_database.h"
#include "chrome/browser/password_manager/password_store.h"

class Profile;
class WebDataService;

// Simple password store implementation that delegates everything to
// the LoginDatabase.
class PasswordStoreDefault : public PasswordStore {
 public:
  // Takes ownership of |login_db|.
  PasswordStoreDefault(LoginDatabase* login_db,
                       Profile* profile,
                       WebDataService* web_data_service);

 protected:
  virtual ~PasswordStoreDefault();

  // Implements PasswordStore interface.
  virtual void Shutdown() OVERRIDE;
  virtual void ReportMetricsImpl() OVERRIDE;
  virtual void AddLoginImpl(const webkit::forms::PasswordForm& form) OVERRIDE;
  virtual void UpdateLoginImpl(
      const webkit::forms::PasswordForm& form) OVERRIDE;
  virtual void RemoveLoginImpl(
      const webkit::forms::PasswordForm& form) OVERRIDE;
  virtual void RemoveLoginsCreatedBetweenImpl(
      const base::Time& delete_begin, const base::Time& delete_end) OVERRIDE;
  virtual void GetLoginsImpl(GetLoginsRequest* request,
                             const webkit::forms::PasswordForm& form) OVERRIDE;
  virtual void GetAutofillableLoginsImpl(GetLoginsRequest* request) OVERRIDE;
  virtual void GetBlacklistLoginsImpl(GetLoginsRequest* request) OVERRIDE;
  virtual bool FillAutofillableLogins(
      std::vector<webkit::forms::PasswordForm*>* forms) OVERRIDE;
  virtual bool FillBlacklistLogins(
      std::vector<webkit::forms::PasswordForm*>* forms) OVERRIDE;

  scoped_refptr<WebDataService> web_data_service_;

 protected:
  inline bool DeleteAndRecreateDatabaseFile() {
    return login_db_->DeleteAndRecreateDatabaseFile();
  }

 private:
  class MigrateHelper;

  // Migrates logins from the WDS to the LoginDatabase.
  void MigrateIfNecessary();

  scoped_ptr<LoginDatabase> login_db_;
  Profile* profile_;

  scoped_ptr<MigrateHelper> migrate_helper_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreDefault);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT_H_
