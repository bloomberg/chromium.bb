// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SYNC_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SYNC_INTERFACE_H_

#include <vector>

#include "components/password_manager/core/browser/password_store_change.h"

namespace password_manager {

// PasswordStore interface for PasswordSyncableService. It provides access to
// synchronous methods of PasswordStore which shouldn't be accessible to other
// classes. These methods are to be called on the PasswordStore background
// thread only.
class PasswordStoreSync {
 public:
  // Finds all non-blacklist PasswordForms, and fills the vector.
  virtual bool FillAutofillableLogins(
      std::vector<autofill::PasswordForm*>* forms) = 0;

  // Finds all blacklist PasswordForms, and fills the vector.
  virtual bool FillBlacklistLogins(
      std::vector<autofill::PasswordForm*>* forms) = 0;

  // Synchronous implementation to add the given login.
  virtual PasswordStoreChangeList AddLoginImpl(
      const autofill::PasswordForm& form) = 0;

  // Synchronous implementation to update the given login.
  virtual PasswordStoreChangeList UpdateLoginImpl(
      const autofill::PasswordForm& form) = 0;

  // Synchronous implementation to remove the given login.
  virtual PasswordStoreChangeList RemoveLoginImpl(
      const autofill::PasswordForm& form) = 0;

  // Notifies observers that password store data may have been changed.
  virtual void NotifyLoginsChanged(const PasswordStoreChangeList& changes) = 0;

 protected:
  virtual ~PasswordStoreSync();
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SYNC_INTERFACE_H_
