// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_DELEGATE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_DELEGATE_H_

#include <string>
#include <vector>

#include "google_apis/gaia/gaia_auth_util.h"

class AccountReconcilor;

namespace signin {

// Base class for AccountReconcilorDelegate.
class AccountReconcilorDelegate {
 public:
  virtual ~AccountReconcilorDelegate() {}

  // Returns true if the reconcilor should reconcile the profile. Defaults to
  // false.
  virtual bool IsReconcileEnabled() const;

  // Returns true if account consistency is enforced (Mirror or Dice).
  // If this is false, reconcile is done, but its results are discarded and no
  // changes to the accounts are made. Defaults to false.
  virtual bool IsAccountConsistencyEnforced() const;

  // Returns true if Reconcile should be aborted when the primary account is in
  // error state. Defaults to false.
  virtual bool ShouldAbortReconcileIfPrimaryHasError() const;

  // Returns the first account to add in the Gaia cookie.
  // If this returns an empty string, the user must be logged out of all
  // accounts.
  virtual std::string GetFirstGaiaAccountForReconcile(
      const std::vector<std::string>& chrome_accounts,
      const std::vector<gaia::ListedAccount>& gaia_accounts,
      const std::string& primary_account,
      bool first_execution) const;

  // Called when reconcile is finished.
  virtual void OnReconcileFinished(const std::string& first_account,
                                   bool reconcile_is_noop) {}

  void set_reconcilor(AccountReconcilor* reconcilor) {
    reconcilor_ = reconcilor;
  }
  AccountReconcilor* reconcilor() { return reconcilor_; }

 private:
  AccountReconcilor* reconcilor_;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_ACCOUNT_RECONCILOR_DELEGATE_H_
