// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REDUNDANT_CREDENTIALS_CLEANER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REDUNDANT_CREDENTIALS_CLEANER_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

class PrefService;

namespace password_manager {

class PasswordStore;

// This class is responsible for deleting blacklisted duplicates. Two
// blacklisted forms are considered equal if they have the same signon_realm.
// Important note: The object will delete itself once the credentials are
// cleared.
// TODO(https://crbug.com/866794): Remove the code once majority of the users
// executed it.
class RedundantCredentialsCleaner : public PasswordStoreConsumer {
 public:
  RedundantCredentialsCleaner(scoped_refptr<PasswordStore> store,
                              PrefService* prefs);
  ~RedundantCredentialsCleaner() override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

 private:
  scoped_refptr<PasswordStore> store_;
  PrefService* prefs_;

  DISALLOW_COPY_AND_ASSIGN(RedundantCredentialsCleaner);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_REDUNDANT_CREDENTIALS_CLEANER_H_