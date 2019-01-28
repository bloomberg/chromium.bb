// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_CODE_GENERATOR_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_CODE_GENERATOR_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "crypto/hmac.h"

namespace chromeos {

// Configuration used to generate and verify Parent Access Code.
class ParentAccessCodeConfig {
 public:
  // To create valid ParentAccessCodeConfig:
  // * |shared_secret| cannot be empty
  // * |code_validity| needs to be in between 30s and 3600s
  // * |clock_drift_tolerance| needs to be between 0 and 1800s
  // The above restrictions are applied to ParentAccessCodeConfig policy that is
  // the main source of this configuration.
  ParentAccessCodeConfig(const std::string& shared_secret,
                         base::TimeDelta code_validity,
                         base::TimeDelta clock_drift_tolerance);
  ParentAccessCodeConfig(const ParentAccessCodeConfig& rhs);
  ParentAccessCodeConfig& operator=(const ParentAccessCodeConfig&);
  ~ParentAccessCodeConfig();

  // Secret shared between child and parent devices.
  const std::string& shared_secret() const { return shared_secret_; }

  // Time that access code is valid for.
  base::TimeDelta code_validity() const { return code_validity_; }

  // The allowed difference between the clock on child and parent devices.
  base::TimeDelta clock_drift_tolerance() const {
    return clock_drift_tolerance_;
  }

 private:
  std::string shared_secret_;
  base::TimeDelta code_validity_;
  base::TimeDelta clock_drift_tolerance_;
};

// Parent Access Code that can be used to authorize various actions on child
// user's device.
// Typical lifetime of the code is 10 minutes and clock difference between
// generating and validating device is half of the code lifetime. Clock
// difference is accounted for during code validation.
class ParentAccessCode {
 public:
  // To create valid ParentAccessCode:
  // * |code| needs to be 6 characters long
  // * |valid_to| needs to be greater than |valid_from|
  ParentAccessCode(const std::string& code,
                   base::Time valid_from,
                   base::Time valid_to);
  ParentAccessCode(const ParentAccessCode&);
  ParentAccessCode& operator=(const ParentAccessCode&);
  ~ParentAccessCode();

  // Parent access code.
  const std::string& code() const { return code_; }

  // Code validity start time.
  base::Time valid_from() const { return valid_from_; }

  // Code expiration time.
  base::Time valid_to() const { return valid_to_; }

  bool operator==(const ParentAccessCode&) const;
  bool operator!=(const ParentAccessCode&) const;

 private:
  std::string code_;
  base::Time valid_from_;
  base::Time valid_to_;
};

// Generates ParentAccessCodes.
class ParentAccessCodeGenerator {
 public:
  // Granularity of which generation and verification are carried out. Should
  // not exceed code validity period.
  static constexpr base::TimeDelta kCodeGranularity =
      base::TimeDelta::FromMinutes(1);

  explicit ParentAccessCodeGenerator(const ParentAccessCodeConfig& config);
  ~ParentAccessCodeGenerator();

  // Generates parent access code from the given |timestamp|. Returns the code
  // if generation was successful.
  base::Optional<ParentAccessCode> Generate(base::Time timestamp);

 private:
  // Configuration used to generate parent access code.
  const ParentAccessCodeConfig config_;

  // Keyed-hash message authentication generator.
  crypto::HMAC hmac_{crypto::HMAC::SHA1};

  DISALLOW_COPY_AND_ASSIGN(ParentAccessCodeGenerator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_PARENT_ACCESS_CODE_GENERATOR_H_
