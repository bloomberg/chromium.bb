// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_WALLET_SIGNIN_HELPER_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_WALLET_SIGNIN_HELPER_DELEGATE_H_

#include <string>
#include <vector>

class GoogleServiceAuthError;

namespace autofill {
namespace wallet {

// An interface that defines the callbacks for objects that
// WalletSigninHelper can return data to.
class WalletSigninHelperDelegate {
 public:
  virtual ~WalletSigninHelperDelegate() {}

  // Called on a successful passive sign-in.
  // |usernames| contains the signed-in user account names (emails).
  virtual void OnPassiveSigninSuccess(const std::vector<std::string>& usernames)
      = 0;

  // Called on a failed passive sign-in; |error| describes the error.
  virtual void OnPassiveSigninFailure(const GoogleServiceAuthError& error) = 0;

  // Called on a successful fetch of the signed-in account name.
  // |usernames| contains the signed-in user account names (emails).
  virtual void OnUserNameFetchSuccess(const std::vector<std::string>& usernames)
      = 0;

  // Called on a failed fetch of the signed-in account name.
  // |error| described the error.
  virtual void OnUserNameFetchFailure(const GoogleServiceAuthError& error) = 0;

  // Called when the Google Wallet cookie value has been retrieved.
  virtual void OnDidFetchWalletCookieValue(const std::string& cookie_value) = 0;
};

}  // namespace wallet
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_WALLET_SIGNIN_HELPER_DELEGATE_H_
