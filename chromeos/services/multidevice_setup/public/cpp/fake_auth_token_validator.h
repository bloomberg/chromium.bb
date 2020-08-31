// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_AUTH_TOKEN_VALIDATOR_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_FAKE_AUTH_TOKEN_VALIDATOR_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/multidevice_setup/public/cpp/auth_token_validator.h"

namespace chromeos {

namespace multidevice_setup {

// Fake AuthTokenValidator implementation for tests.
class FakeAuthTokenValidator : public AuthTokenValidator {
 public:
  FakeAuthTokenValidator();
  ~FakeAuthTokenValidator() override;

  // AuthTokenValidator:
  bool IsAuthTokenValid(const std::string& auth_token) override;

  void set_expected_auth_token(const std::string& expected_auth_token) {
    expected_auth_token_ = expected_auth_token;
  }

 private:
  base::Optional<std::string> expected_auth_token_;

  DISALLOW_COPY_AND_ASSIGN(FakeAuthTokenValidator);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_AUTH_TOKEN_VALIDATOR_H_
