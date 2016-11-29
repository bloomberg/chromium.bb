// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detector.h"

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"

namespace password_manager {

namespace {
// Minimum number of characters in a password for finding it as password reuse.
// It does not make sense to consider short strings for password reuse, since it
// is quite likely that they are parts of common words.
constexpr size_t kMinPasswordLengthToCheck = 8;
}

PasswordReuseDetector::PasswordReuseDetector() {}

PasswordReuseDetector::~PasswordReuseDetector() {}

void PasswordReuseDetector::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  passwords_.clear();
  for (const auto& form : results) {
    const base::string16& password = form->password_value;
    if (password.size() < kMinPasswordLengthToCheck)
      continue;
    GURL signon_realm(form->signon_realm);
    const std::string domain = GetRegistryControlledDomain(signon_realm);
    passwords_[password].insert(domain);
  }
}

void PasswordReuseDetector::CheckReuse(
    const base::string16& input,
    const std::string& domain,
    PasswordReuseDetectorConsumer* consumer) {
  // TODO(crbug.com/668155): Optimize password look-up.
  DCHECK(consumer);
  const std::string registry_controlled_domain =
      GetRegistryControlledDomain(GURL(domain));
  for (size_t i = 0; i + kMinPasswordLengthToCheck <= input.size(); ++i) {
    const base::string16 input_suffix = input.substr(i);
    auto passwords_it = passwords_.find(input_suffix);
    if (passwords_it == passwords_.end())
      continue;

    // Password found, check whether it's saved on the same domain.
    const std::set<std::string>& domains = passwords_it->second;
    DCHECK(!domains.empty());
    if (domains.find(registry_controlled_domain) == domains.end()) {
      // Return only one domain.
      const std::string& saved_domain = *domains.begin();
      consumer->OnReuseFound(input_suffix, saved_domain);
      return;
    }
  }
}

}  // namespace password_manager
