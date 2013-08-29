// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_UTILITY_STUB_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_UTILITY_STUB_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "chrome/browser/managed_mode/managed_user_registration_utility.h"
#include "google_apis/gaia/google_service_auth_error.h"

class ManagedUserRegistrationUtilityStub
    : public ManagedUserRegistrationUtility {
 public:
  ManagedUserRegistrationUtilityStub();
  virtual ~ManagedUserRegistrationUtilityStub();

  virtual void Register(const std::string& managed_user_id,
                        const ManagedUserRegistrationInfo& info,
                        const RegistrationCallback& callback) OVERRIDE;

  bool register_was_called() { return register_was_called_; }

  std::string managed_user_id() { return managed_user_id_; }

  string16 display_name() { return display_name_; }

  std::string master_key() { return master_key_; }

  void RunSuccessCallback(const std::string& token);
  void RunFailureCallback(GoogleServiceAuthError::State error);

 private:
   RegistrationCallback callback_;
   bool register_was_called_;
   std::string managed_user_id_;
   string16 display_name_;
   std::string master_key_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserRegistrationUtilityStub);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_UTILITY_STUB_H_
