// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {

class PasswordReuseDetectorConsumer;

// Comparator that compares reversed strings.
struct ReverseStringLess {
  bool operator()(const base::string16& lhs, const base::string16& rhs) const;
};

// Per-profile class responsible for detection of password reuse, i.e. that the
// user input on some site contains the password saved on another site.
// It receives saved passwords through PasswordStoreConsumer interface.
// It stores passwords in memory and CheckReuse() can be used for finding
// a password reuse.
class PasswordReuseDetector : public PasswordStoreConsumer {
 public:
  PasswordReuseDetector();
  ~PasswordReuseDetector() override;

  // PasswordStoreConsumer
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  // Add new or updated passwords from |changes| to internal password index.
  void OnLoginsChanged(const PasswordStoreChangeList& changes);

  // Checks that some suffix of |input| equals to a password saved on another
  // registry controlled domain than |domain|.
  // If such suffix is found, |consumer|->OnReuseFound() is called on the same
  // thread on which this method is called.
  // |consumer| should not be null.
  void CheckReuse(const base::string16& input,
                  const std::string& domain,
                  PasswordReuseDetectorConsumer* consumer);

 private:
  using passwords_iterator = std::map<base::string16,
                                      std::set<std::string>,
                                      ReverseStringLess>::const_iterator;

  // Add password from |form| to |passwords_|.
  void AddPassword(const autofill::PasswordForm& form);

  // Returns the iterator to |passwords_| that corresponds to the longest key in
  // |passwords_| that is a suffix of |input|. Returns passwords_.end() in case
  // when no key in |passwords_| is a prefix of |input|.
  passwords_iterator FindSavedPassword(const base::string16& input);

  // Contains all passwords.
  // A key is a password.
  // A value is a set of registry controlled domains on which the password
  // saved.
  std::map<base::string16, std::set<std::string>, ReverseStringLess> passwords_;

  // Number of passwords in |passwords_|, each password is calculated the number
  // of times how many different sites it's saved on.
  int saved_passwords_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PasswordReuseDetector);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_H_
