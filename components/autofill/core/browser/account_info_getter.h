// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ACCOUNT_INFO_GETTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ACCOUNT_INFO_GETTER_H_

#include <string>

namespace autofill {

// Interface to get account information in Autofill.
class AccountInfoGetter {
 public:
  // Returns the account id of the active signed-in user irrespective of
  // whether they enabled sync or not.
  virtual std::string GetActiveSignedInAccountId() const = 0;

  // Returns true - When user is both signed-in and enabled sync.
  // Returns false - When user is not signed-in or does not have sync the
  // feature enabled.
  virtual bool IsSyncFeatureEnabled() const = 0;

 protected:
  virtual ~AccountInfoGetter() {}
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ACCOUNT_INFO_GETTER_H_
