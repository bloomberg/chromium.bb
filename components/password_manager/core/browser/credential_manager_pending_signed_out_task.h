// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_SIGNED_OUT_TASK_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_SIGNED_OUT_TASK_H_

#include <set>
#include <string>

#include "base/memory/scoped_vector.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

class GURL;

namespace autofill {
struct PasswordForm;
}  // namespace autofill

namespace password_manager {

class PasswordStore;

// Handles sign-out completion and retrieves embedder-dependent services.
class CredentialManagerPendingSignedOutTaskDelegate {
 public:
  // Retrieves the PasswordStore.
  virtual PasswordStore* GetPasswordStore() = 0;

  // Finishes sign-out tasks.
  virtual void DoneSigningOut() = 0;
};

// Notifies the password store that a list of origins have been signed out.
class CredentialManagerPendingSignedOutTask : public PasswordStoreConsumer {
 public:
  CredentialManagerPendingSignedOutTask(
      CredentialManagerPendingSignedOutTaskDelegate* delegate,
      const GURL& origin);
  ~CredentialManagerPendingSignedOutTask() override;

  // Adds an origin to be signed out.
  void AddOrigin(const GURL& origin);

  // PasswordStoreConsumer implementation.
  void OnGetPasswordStoreResults(
      ScopedVector<autofill::PasswordForm> results) override;

 private:
  CredentialManagerPendingSignedOutTaskDelegate* const delegate_;  // Weak.
  std::set<std::string> origins_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerPendingSignedOutTask);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PENDING_SIGNED_OUT_TASK_H_
