// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_USER_CONTEXT_H_
#define CHROMEOS_LOGIN_AUTH_USER_CONTEXT_H_

#include <string>

#include "chromeos/chromeos_export.h"
#include "chromeos/login/auth/key.h"
#include "components/user_manager/user_type.h"

namespace chromeos {

// Information that is passed around while authentication is in progress. The
// credentials may consist of a |user_id_|, |key_| pair or a GAIA |auth_code_|.
// The |user_id_hash_| is used to locate the user's home directory
// mount point for the user. It is set when the mount has been completed.
class CHROMEOS_EXPORT UserContext {
 public:
  // The authentication flow used during sign-in.
  enum AuthFlow {
    // Online authentication against GAIA. GAIA did not redirect to a SAML IdP.
    AUTH_FLOW_GAIA_WITHOUT_SAML,
    // Online authentication against GAIA. GAIA redirected to a SAML IdP.
    AUTH_FLOW_GAIA_WITH_SAML,
    // Offline authentication against a cached key.
    AUTH_FLOW_OFFLINE,
    // Offline authentication using and Easy unlock device (e.g. a phone).
    AUTH_FLOW_EASY_UNLOCK
  };

  UserContext();
  UserContext(const UserContext& other);
  explicit UserContext(const std::string& user_id);
  UserContext(user_manager::UserType user_type, const std::string& user_id);
  ~UserContext();

  bool operator==(const UserContext& context) const;
  bool operator!=(const UserContext& context) const;

  const std::string& GetUserID() const;
  const Key* GetKey() const;
  Key* GetKey();
  const std::string& GetAuthCode() const;
  const std::string& GetUserIDHash() const;
  bool IsUsingOAuth() const;
  AuthFlow GetAuthFlow() const;
  user_manager::UserType GetUserType() const;
  const std::string& GetPublicSessionLocale() const;
  const std::string& GetPublicSessionInputMethod() const;

  bool HasCredentials() const;

  void SetUserID(const std::string& user_id);
  void SetKey(const Key& key);
  void SetAuthCode(const std::string& auth_code);
  void SetUserIDHash(const std::string& user_id_hash);
  void SetIsUsingOAuth(bool is_using_oauth);
  void SetAuthFlow(AuthFlow auth_flow);
  void SetUserType(user_manager::UserType user_type);
  void SetPublicSessionLocale(const std::string& locale);
  void SetPublicSessionInputMethod(const std::string& input_method);

  void ClearSecrets();

 private:
  std::string user_id_;
  Key key_;
  std::string auth_code_;
  std::string user_id_hash_;
  bool is_using_oauth_;
  AuthFlow auth_flow_;
  user_manager::UserType user_type_;
  std::string public_session_locale_;
  std::string public_session_input_method_;
};

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_USER_CONTEXT_H_
