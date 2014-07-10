// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_TEST_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_TEST_PASSWORD_STORE_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/password_manager/core/browser/password_store.h"

namespace content {
class BrowserContext;
}

namespace password_manager {

// A very simple PasswordStore implementation that keeps all of the passwords
// in memory and does all its manipulations on the main thread. Since this
// is only used for testing, only the parts of the interface that are needed
// for testing have been implemented.
class TestPasswordStore : public PasswordStore {
 public:
  TestPasswordStore();

  typedef std::map<std::string /* signon_realm */,
                   std::vector<autofill::PasswordForm> > PasswordMap;

  const PasswordMap& stored_passwords() const;
  void Clear();

  // Returns true if no passwords are stored in the store. Note that this is not
  // as simple as asking whether stored_passwords().empty(), because the map can
  // have entries of size 0.
  bool IsEmpty() const;

 protected:
  virtual ~TestPasswordStore();

  // Helper function to determine if forms are considered equivalent.
  bool FormsAreEquivalent(const autofill::PasswordForm& lhs,
                          const autofill::PasswordForm& rhs);

  // PasswordStore interface
  virtual PasswordStoreChangeList AddLoginImpl(
      const autofill::PasswordForm& form) OVERRIDE;
  virtual PasswordStoreChangeList UpdateLoginImpl(
      const autofill::PasswordForm& form) OVERRIDE;
  virtual PasswordStoreChangeList RemoveLoginImpl(
      const autofill::PasswordForm& form) OVERRIDE;
  virtual void GetLoginsImpl(
      const autofill::PasswordForm& form,
      PasswordStore::AuthorizationPromptPolicy prompt_policy,
      const ConsumerCallbackRunner& runner) OVERRIDE;
  virtual void WrapModificationTask(ModificationTask task) OVERRIDE;

  // Unused portions of PasswordStore interface
  virtual void ReportMetricsImpl(const std::string& sync_username) OVERRIDE {}
  virtual PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time begin,
      base::Time end) OVERRIDE;
  virtual PasswordStoreChangeList RemoveLoginsSyncedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) OVERRIDE;
  virtual void GetAutofillableLoginsImpl(
      PasswordStore::GetLoginsRequest* request) OVERRIDE {}
  virtual void GetBlacklistLoginsImpl(
      PasswordStore::GetLoginsRequest* request) OVERRIDE {}
  virtual bool FillAutofillableLogins(
      std::vector<autofill::PasswordForm*>* forms) OVERRIDE;
  virtual bool FillBlacklistLogins(
      std::vector<autofill::PasswordForm*>* forms) OVERRIDE;

 private:
  PasswordMap stored_passwords_;

  DISALLOW_COPY_AND_ASSIGN(TestPasswordStore);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_TEST_PASSWORD_STORE_H_
