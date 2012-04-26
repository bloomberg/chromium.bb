// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT_H_
#pragma once

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/password_manager/login_database.h"
#include "chrome/browser/password_manager/password_store.h"

class Profile;

// Simple password store implementation that delegates everything to
// the LoginDatabase.
class PasswordStoreDefault : public PasswordStore {
 public:
  // Takes ownership of |login_db|.
  PasswordStoreDefault(LoginDatabase* login_db,
                       Profile* profile);

 protected:
  virtual ~PasswordStoreDefault();

  // Implements RefCountedProfileKeyedService.
  virtual void ShutdownOnUIThread() OVERRIDE;

  // Implements PasswordStore interface.
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

 protected:
  inline bool DeleteAndRecreateDatabaseFile() {
    return login_db_->DeleteAndRecreateDatabaseFile();
  }

 private:
  scoped_ptr<LoginDatabase> login_db_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreDefault);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_DEFAULT_H_
