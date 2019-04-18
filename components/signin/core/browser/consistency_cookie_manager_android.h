// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_ANDROID_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_ANDROID_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/signin_metrics.h"

class SigninClient;

namespace signin {

// The ConsistencyCookieManagerAndroid checks if:
// - the account reconcilor is running
// - the accounts on the device are updating
// - the user has started to interact with device account settings (from Chrome)
// If one of these conditions is true, then this object sets a cookie on Gaia
// with a "Updating" value.
//
// Otherwise the value of the cookie is "Consistent" if the accounts are
// consistent (web accounts match device accounts) or "Inconsistent".
class ConsistencyCookieManagerAndroid : public AccountReconcilor::Observer {
 public:
  ConsistencyCookieManagerAndroid(SigninClient* signin_client,
                                  AccountReconcilor* reconcilor);

  ~ConsistencyCookieManagerAndroid() override;

 private:
  // AccountReconcilor::Observer:
  void OnStateChanged(signin_metrics::AccountReconcilorState state) override;

  void UpdateCookie();

  signin_metrics::AccountReconcilorState account_reconcilor_state_ =
      signin_metrics::ACCOUNT_RECONCILOR_OK;
  SigninClient* signin_client_ = nullptr;
  ScopedObserver<AccountReconcilor, AccountReconcilor::Observer>
      account_reconcilor_observer_;

  DISALLOW_COPY_AND_ASSIGN(ConsistencyCookieManagerAndroid);
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_CONSISTENCY_COOKIE_MANAGER_ANDROID_H_
