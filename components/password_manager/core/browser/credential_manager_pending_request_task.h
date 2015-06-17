// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_vector.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "url/gurl.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

namespace password_manager {

struct CredentialInfo;
class PasswordManagerClient;

// Sends credentials retrieved from the PasswordStore to CredentialManager API
// clients and retrieves embedder-dependent information.
class CredentialManagerPendingRequestTaskDelegate {
 public:
  // Determines whether zero-click sign-in is allowed.
  virtual bool IsZeroClickAllowed() const = 0;

  // Retrieves the current page origin.
  virtual GURL GetOrigin() const = 0;

  // Returns the PasswordManagerClient.
  virtual PasswordManagerClient* client() const = 0;

  // Sends a credential to JavaScript.
  virtual void SendCredential(int id, const CredentialInfo& credential) = 0;
};

// Retrieves credentials from the PasswordStore.
class CredentialManagerPendingRequestTask : public PasswordStoreConsumer {
 public:
  CredentialManagerPendingRequestTask(
      CredentialManagerPendingRequestTaskDelegate* delegate,
      int request_id,
      bool request_zero_click_only,
      const GURL& request_origin,
      const std::vector<GURL>& request_federations);
  ~CredentialManagerPendingRequestTask() override;

  int id() const { return id_; }
  const GURL& origin() const { return origin_; }

  // PasswordStoreConsumer implementation.
  void OnGetPasswordStoreResults(
      ScopedVector<autofill::PasswordForm> results) override;

 private:
  CredentialManagerPendingRequestTaskDelegate* delegate_;  // Weak;
  const int id_;
  const bool zero_click_only_;
  const GURL origin_;
  std::set<std::string> federations_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerPendingRequestTask);
};

}  // namespace password_manager
