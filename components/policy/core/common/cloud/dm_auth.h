// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_DM_AUTH_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_DM_AUTH_H_

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "components/policy/policy_export.h"

namespace policy {

// The enum type for the token used for authentication. Explicit values are
// set to allow easy identification of value from the logs.
enum class DMAuthTokenType {
  kNoAuth = 0,
  kGaia = 1,
  kDm = 2,
  kEnrollment = 3,
  kOauth = 4,
};

// Class that encapsulates different authentication methods to interact with
// device management service.
// We currently have 4 methods for authentication:
// * OAuth token, that is passes as a part of URL
// * GAIA token, an OAuth token passed as Authorization: GoogleLogin header.
// * Enrollment token, provided by installation configuration, passed as
//       Authorization: GoogleEnrollmentToken header
// * DMToken, created during Register request, passed as
//     Authorization: GoogleDMToken header
// Also, several requests require no authentication.
class POLICY_EXPORT DMAuth {
 public:
  // Static methods for creating DMAuth instances:
  static std::unique_ptr<DMAuth> FromDMToken(const std::string& dm_token);
  static std::unique_ptr<DMAuth> FromGaiaToken(const std::string& gaia_token);
  static std::unique_ptr<DMAuth> FromOAuthToken(const std::string& oauth_token);
  static std::unique_ptr<DMAuth> FromEnrollmentToken(const std::string& token);
  static std::unique_ptr<DMAuth> NoAuth();

  DMAuth();
  ~DMAuth();

  // Creates a copy of DMAuth.
  std::unique_ptr<DMAuth> Clone() const;

  // Checks if no authentication is provided.
  bool empty() const { return token_type_ == DMAuthTokenType::kNoAuth; }
  bool Equals(const DMAuth& other) const;

  std::string gaia_token() const {
    DCHECK_EQ(DMAuthTokenType::kGaia, token_type_);
    return token_;
  }
  bool has_gaia_token() const { return token_type_ == DMAuthTokenType::kGaia; }
  std::string dm_token() const {
    DCHECK_EQ(DMAuthTokenType::kDm, token_type_);
    return token_;
  }
  bool has_dm_token() const { return token_type_ == DMAuthTokenType::kDm; }
  std::string enrollment_token() const {
    DCHECK_EQ(DMAuthTokenType::kEnrollment, token_type_);
    return token_;
  }
  bool has_enrollment_token() const {
    return token_type_ == DMAuthTokenType::kEnrollment;
  }
  std::string oauth_token() const {
    DCHECK_EQ(DMAuthTokenType::kOauth, token_type_);
    return token_;
  }
  bool has_oauth_token() const {
    return token_type_ == DMAuthTokenType::kOauth;
  }
  DMAuthTokenType token_type() const { return token_type_; }

 private:
  DMAuth(const std::string& token, DMAuthTokenType token_type);
  DMAuth& operator=(const DMAuth&) = default;

  std::string token_;
  DMAuthTokenType token_type_ = DMAuthTokenType::kNoAuth;

  DISALLOW_COPY(DMAuth);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_DM_AUTH_H_
