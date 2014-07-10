// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_DEFAULT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_DEFAULT_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {

// Simple password store implementation that delegates everything to
// the LoginDatabase.
class PasswordStoreDefault : public PasswordStore {
 public:
  // Takes ownership of |login_db|.
  PasswordStoreDefault(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner,
      LoginDatabase* login_db);

 protected:
  virtual ~PasswordStoreDefault();

  // Implements PasswordStore interface.
  virtual void ReportMetricsImpl(const std::string& sync_username) OVERRIDE;
  virtual PasswordStoreChangeList AddLoginImpl(
      const autofill::PasswordForm& form) OVERRIDE;
  virtual PasswordStoreChangeList UpdateLoginImpl(
      const autofill::PasswordForm& form) OVERRIDE;
  virtual PasswordStoreChangeList RemoveLoginImpl(
      const autofill::PasswordForm& form) OVERRIDE;
  virtual PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) OVERRIDE;
  virtual PasswordStoreChangeList RemoveLoginsSyncedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) OVERRIDE;
  virtual void GetLoginsImpl(
      const autofill::PasswordForm& form,
      AuthorizationPromptPolicy prompt_policy,
      const ConsumerCallbackRunner& callback_runner) OVERRIDE;
  virtual void GetAutofillableLoginsImpl(GetLoginsRequest* request) OVERRIDE;
  virtual void GetBlacklistLoginsImpl(GetLoginsRequest* request) OVERRIDE;
  virtual bool FillAutofillableLogins(
      std::vector<autofill::PasswordForm*>* forms) OVERRIDE;
  virtual bool FillBlacklistLogins(
      std::vector<autofill::PasswordForm*>* forms) OVERRIDE;

 protected:
  inline bool DeleteAndRecreateDatabaseFile() {
    return login_db_->DeleteAndRecreateDatabaseFile();
  }

 private:
  scoped_ptr<LoginDatabase> login_db_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreDefault);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_DEFAULT_H_
