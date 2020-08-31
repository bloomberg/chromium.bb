// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SECURITY_TOKEN_PIN_CONSTANTS_H_
#define CHROMEOS_COMPONENTS_SECURITY_TOKEN_PIN_CONSTANTS_H_

// This header contains types related to the security token PIN requests.

namespace chromeos {
namespace security_token_pin {

// Type of the information asked from the user during a security token PIN
// request.
// Must be kept in sync with
// chrome/browser/resources/chromeos/login/components/oobe_types.js.
enum class CodeType {
  kPin = 0,
  kPuk = 1,
};

// Error to be displayed in the security token PIN request.
// Must be kept in sync with
// chrome/browser/resources/chromeos/login/components/oobe_types.js.
enum class ErrorLabel {
  kNone = 0,
  kUnknown = 1,
  kInvalidPin = 2,
  kInvalidPuk = 3,
  kMaxAttemptsExceeded = 4,
};

}  // namespace security_token_pin
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SECURITY_TOKEN_PIN_CONSTANTS_H_
