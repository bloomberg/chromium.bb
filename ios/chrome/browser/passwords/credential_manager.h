// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_CREDENTIAL_MANAGER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_CREDENTIAL_MANAGER_H_

#include "components/password_manager/core/browser/credential_manager_impl.h"

namespace credential_manager {

// Owned by PasswordController. It is responsible for registering and handling
// callbacks for JS methods |navigator.credentials.*|.
// TODO(crbug.com/435047): Implement JSCredentialManager responsible for
// sending results back to website. Expected flow of CredentialManager class:
// 0. Add script command callbacks, initialize JSCredentialManager
// 1. A command is sent from JavaScript to the browser.
// 2. HandleScriptCommand is called, it parses the message and constructs a
//     OnceCallback to be passed as parameter to proper CredentialManagerImpl
//     method. |requestId| field from received JS message is bound to
//     constructed OnceCallback.
// 3. CredentialManagerImpl method is invoked, performs some logic with
//     PasswordStore, calls passed OnceCallback with result.
// 4. The OnceCallback uses JSCredentialManager to send result back to the
//     website.
class CredentialManager {
 public:
  explicit CredentialManager(password_manager::PasswordManagerClient* client);
  ~CredentialManager();

 private:
  // HandleScriptCommand parses JSON message and invokes Get, Store or
  // PreventSilentAccess on CredentialManagerImpl.
  bool HandleScriptCommand(const base::DictionaryValue& json,
                           const GURL& origin_url,
                           bool user_is_interacting);

  // Passed as callback to CredentialManagerImpl::Get.
  void SendGetResponse(
      int request_id,
      password_manager::CredentialManagerError error,
      const base::Optional<password_manager::CredentialInfo>& info);
  // Passed as callback to CredentialManagerImpl::PreventSilentAccess.
  void SendPreventSilentAccessResponse(int request_id);
  // Passed as callback to CredentialManagerImpl::Store.
  void SendStoreResponse(int request_id);

  password_manager::CredentialManagerImpl impl_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManager);
};

}  // namespace credential_manager

#endif  // IOS_CHROME_BROWSER_PASSWORDS_CREDENTIAL_MANAGER_H_
