// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_WALLET_SIGNIN_HELPER_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_WALLET_SIGNIN_HELPER_DELEGATE_H_

#include <string>

class GoogleServiceAuthError;

namespace autofill {
namespace wallet {

// An interface that defines the callbacks for objects that
// WalletSigninHelper can return data to.
class WalletSigninHelperDelegate {
 public:
  virtual ~WalletSigninHelperDelegate() {}

  // Called on a successful passive sign-in.
  // |username| is the signed-in user account name (email).
  virtual void OnPassiveSigninSuccess(const std::string& username) = 0;

  // Called on a failed passive sign-in; |error| describes the error.
  virtual void OnPassiveSigninFailure(const GoogleServiceAuthError& error) = 0;

  // Called on a successful automatic sign-in.
  // |username| is the signed-in user account name (email).
  virtual void OnAutomaticSigninSuccess(const std::string& username) = 0;

  // Called on a failed automatic sign-in; |error| describes the error.
  virtual void OnAutomaticSigninFailure(
      const GoogleServiceAuthError& error) = 0;

  // Called on a successful fetch of the signed-in account name.
  // |username| is the signed-in user account name (email).
  virtual void OnUserNameFetchSuccess(const std::string& username) = 0;

  // Called on a failed fetch of the signed-in account name.
  // |error| described the error.
  virtual void OnUserNameFetchFailure(const GoogleServiceAuthError& error) = 0;
};

}  // namespace wallet
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_WALLET_SIGNIN_HELPER_DELEGATE_H_
