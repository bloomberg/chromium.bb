// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_SIGNIN_FLOW_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_SIGNIN_FLOW_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "services/identity/public/interfaces/identity_manager.mojom.h"

class Profile;

namespace extensions {

// IdentitySigninFlow is a controller class to do a sign-in flow for an
// interactive Identity API call. The UI is launched through the LoginUIService.
// When the flow completes, the delegate is notified, and on success will
// be given an OAuth2 login refresh token.
class IdentitySigninFlow {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}
    // Called when the flow has completed successfully.
    virtual void SigninSuccess() = 0;
    // Called when the flow has failed.
    virtual void SigninFailed() = 0;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  IdentitySigninFlow(Delegate* delegate,
                     Profile* profile);
  ~IdentitySigninFlow();

  // Starts the flow. Should only be called once.
  void Start();

 private:
  // Invoked by the IdentityManager when the primary account is available.
  void OnPrimaryAccountAvailable(const AccountInfo& account_info,
                                 const identity::AccountState& account_state);

  Delegate* delegate_;
  Profile* profile_;
  identity::mojom::IdentityManagerPtr identity_manager_;

  DISALLOW_COPY_AND_ASSIGN(IdentitySigninFlow);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_SIGNIN_FLOW_H_
