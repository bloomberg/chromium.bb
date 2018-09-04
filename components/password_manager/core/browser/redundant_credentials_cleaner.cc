// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/redundant_credentials_cleaner.h"

#include <set>
#include <string>
#include <utility>

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

RedundantCredentialsCleaner::RedundantCredentialsCleaner(
    scoped_refptr<PasswordStore> store,
    PrefService* prefs)
    : store_(std::move(store)), prefs_(prefs) {
  store_->GetBlacklistLogins(this);
}

RedundantCredentialsCleaner::~RedundantCredentialsCleaner() = default;

void RedundantCredentialsCleaner::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  std::set<std::string> signon_realms;
  for (const auto& form : results) {
    DCHECK(form->blacklisted_by_user);
    if (!signon_realms.insert(form->signon_realm).second) {
      // |results| already contain a form with the same signon_realm.
      store_->RemoveLogin(*form);
    }
  }
  prefs_->SetBoolean(prefs::kDuplicatedBlacklistedCredentialsRemoved,
                     results.size() == signon_realms.size());
  delete this;
}

}  // namespace password_manager