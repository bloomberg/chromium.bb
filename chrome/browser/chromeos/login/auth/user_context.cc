// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth/user_context.h"

#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"

namespace chromeos {

UserContext::UserContext() : does_need_password_hashing_(true),
                             is_using_oauth_(true),
                             auth_flow_(AUTH_FLOW_OFFLINE) {
}

UserContext::UserContext(const UserContext& other)
    : user_id_(other.user_id_),
      password_(other.password_),
      does_need_password_hashing_(other.does_need_password_hashing_),
      key_label_(other.key_label_),
      auth_code_(other.auth_code_),
      user_id_hash_(other.user_id_hash_),
      is_using_oauth_(other.is_using_oauth_),
      auth_flow_(other.auth_flow_) {
}

UserContext::UserContext(const std::string& user_id)
    : user_id_(login::CanonicalizeUserID(user_id)),
      does_need_password_hashing_(true),
      is_using_oauth_(true),
      auth_flow_(AUTH_FLOW_OFFLINE) {
}

UserContext::~UserContext() {
}

bool UserContext::operator==(const UserContext& context) const {
  return context.user_id_ == user_id_ &&
         context.password_ == password_ &&
         context.does_need_password_hashing_ == does_need_password_hashing_ &&
         context.key_label_ == key_label_ &&
         context.auth_code_ == auth_code_ &&
         context.user_id_hash_ == user_id_hash_ &&
         context.is_using_oauth_ == is_using_oauth_ &&
         context.auth_flow_ == auth_flow_;
}

const std::string& UserContext::GetUserID() const {
  return user_id_;
}

const std::string& UserContext::GetPassword() const {
  return password_;
}

bool UserContext::DoesNeedPasswordHashing() const {
  return does_need_password_hashing_;
}

const std::string& UserContext::GetKeyLabel() const {
  return key_label_;
}

const std::string& UserContext::GetAuthCode() const {
  return auth_code_;
}

const std::string& UserContext::GetUserIDHash() const {
  return user_id_hash_;
}

bool UserContext::IsUsingOAuth() const {
  return is_using_oauth_;
}

UserContext::AuthFlow UserContext::GetAuthFlow() const {
  return auth_flow_;
}

bool UserContext::HasCredentials() const {
  return (!user_id_.empty() && !password_.empty()) || !auth_code_.empty();
}

void UserContext::SetUserID(const std::string& user_id) {
  user_id_ = login::CanonicalizeUserID(user_id);
}

void UserContext::SetPassword(const std::string& password) {
  password_ = password;
}

void UserContext::SetDoesNeedPasswordHashing(bool does_need_password_hashing) {
  does_need_password_hashing_ = does_need_password_hashing;
}

void UserContext::SetKeyLabel(const std::string& key_label) {
  key_label_ = key_label;
}

void UserContext::SetAuthCode(const std::string& auth_code) {
  auth_code_ = auth_code;
}

void UserContext::SetUserIDHash(const std::string& user_id_hash) {
  user_id_hash_ = user_id_hash;
}

void UserContext::SetIsUsingOAuth(bool is_using_oauth) {
  is_using_oauth_ = is_using_oauth;
}

void UserContext::SetAuthFlow(AuthFlow auth_flow) {
  auth_flow_ = auth_flow;
}

void UserContext::ClearSecrets() {
  password_.clear();
  auth_code_.clear();
}

}  // namespace chromeos
