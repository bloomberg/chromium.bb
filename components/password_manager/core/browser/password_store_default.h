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
  // The |login_db| must not have been Init()-ed yet. It will be initialized in
  // a deferred manner on the DB thread.
  PasswordStoreDefault(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
      scoped_refptr<base::SingleThreadTaskRunner> db_thread_runner,
      scoped_ptr<LoginDatabase> login_db);

  bool Init(const syncer::SyncableService::StartSyncFlare& flare) override;

  // To be used only for testing.
  LoginDatabase* login_db() const { return login_db_.get(); }

 protected:
  ~PasswordStoreDefault() override;

  // Opens |login_db_| on the DB thread.
  void InitOnDBThread();

  // Implements PasswordStore interface.
  void ReportMetricsImpl(const std::string& sync_username,
                         bool custom_passphrase_sync_enabled) override;
  PasswordStoreChangeList AddLoginImpl(
      const autofill::PasswordForm& form) override;
  PasswordStoreChangeList UpdateLoginImpl(
      const autofill::PasswordForm& form) override;
  PasswordStoreChangeList RemoveLoginImpl(
      const autofill::PasswordForm& form) override;
  PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) override;
  PasswordStoreChangeList RemoveLoginsSyncedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) override;
  ScopedVector<autofill::PasswordForm> FillMatchingLogins(
      const autofill::PasswordForm& form,
      AuthorizationPromptPolicy prompt_policy) override;
  void GetAutofillableLoginsImpl(scoped_ptr<GetLoginsRequest> request) override;
  void GetBlacklistLoginsImpl(scoped_ptr<GetLoginsRequest> request) override;
  bool FillAutofillableLogins(
      ScopedVector<autofill::PasswordForm>* forms) override;
  bool FillBlacklistLogins(
      ScopedVector<autofill::PasswordForm>* forms) override;

 protected:
  inline bool DeleteAndRecreateDatabaseFile() {
    return login_db_->DeleteAndRecreateDatabaseFile();
  }

 private:
  // The login SQL database. The LoginDatabase instance is received via the
  // in an uninitialized state, so as to allow injecting mocks, then Init() is
  // called on the DB thread in a deferred manner. If opening the DB fails,
  // |login_db_| will be reset and stay NULL for the lifetime of |this|.
  scoped_ptr<LoginDatabase> login_db_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreDefault);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_DEFAULT_H_
