// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_CLIENT_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_CLIENT_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"

class GURL;

namespace password_manager {

struct CredentialInfo;

class CredentialManagerDispatcher {
 public:
  CredentialManagerDispatcher() {}
  virtual ~CredentialManagerDispatcher() {}

  // Called in response to an IPC from the renderer, triggered by a page's call
  // to 'navigator.credentials.notifyFailedSignIn'.
  virtual void OnNotifyFailedSignIn(int request_id, const CredentialInfo&) {}

  // Called in response to an IPC from the renderer, triggered by a page's call
  // to 'navigator.credentials.notifySignedIn'.
  virtual void OnNotifySignedIn(int request_id, const CredentialInfo&) {}

  // Called in response to an IPC from the renderer, triggered by a page's call
  // to 'navigator.credentials.notifySignedOut'.
  virtual void OnNotifySignedOut(int request_id) {}

  // Called in response to an IPC from the renderer, triggered by a page's call
  // to 'navigator.credentials.request'.
  virtual void OnRequestCredential(int request_id,
                                   bool zero_click_only,
                                   const std::vector<GURL>& federations) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CredentialManagerDispatcher);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_CLIENT_H_
