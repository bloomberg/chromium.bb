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
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace password_manager {

// Callback interface for receiving a password reuse event.
class PasswordReuseDetectorConsumer {
 public:
  // Called when a password reuse is found.
  // |saved_domain| is the domain on which |password| is saved.
  virtual void OnReuseFound(const base::string16& password,
                            const std::string& saved_domain) = 0;
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

  // Checks that some suffix of |input| equals to a password saved on another
  // registry controlled domain than |domain|.
  // If such suffix is found, |consumer|->OnReuseFound() is called on the same
  // thread on which this method is called.
  // |consumer| should not be null.
  void CheckReuse(const base::string16& input,
                  const std::string& domain,
                  PasswordReuseDetectorConsumer* consumer);

 private:
  // Contains all passwords.
  // A key is a password.
  // A value is a set of registry controlled domains on which the password
  // saved.
  std::map<base::string16, std::set<std::string>> passwords_;

  DISALLOW_COPY_AND_ASSIGN(PasswordReuseDetector);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_REUSE_DETECTOR_H_
