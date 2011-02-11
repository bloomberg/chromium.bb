// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Use MockGaiaAuthenticator to test your application by faking a login session.
// This mock object should be initialized with the response you expect it to
// return, and then can be used in exactly the same way as the real
// GaiaAuthenticator.

#include "chrome/test/sync/engine/mock_gaia_authenticator.h"

using std::string;

namespace browser_sync {

MockGaiaAuthenticator::MockGaiaAuthenticator(const char* user_agent,
                                             const char* service_id,
                                             const char* gaia_url) :
    should_save_credentials_(false), current_user_("") {
  // This discards user_agent, service_id, gaia_url since the mock object
  // doesn't care about them.
}

MockGaiaAuthenticator::~MockGaiaAuthenticator() {}

// Add a mock user to internal list of users.
void MockGaiaAuthenticator::AddMockUser(MockUser mock_user) {
  mock_credentials_[mock_user.email] = mock_user;
}

// A convenience method to add a mock user to internal list of users.
void MockGaiaAuthenticator::AddMockUser(string email, string passwd,
                                        string auth_token, string lsid,
                                        string sid,
                                        enum gaia::AuthenticationError
                                        auth_error, string error_url,
                                        string captcha_token,
                                        string captcha_url) {
  MockUser mock_user;
  mock_user.email = email;
  mock_user.passwd = passwd;
  mock_user.auth_token = auth_token;
  mock_user.lsid = lsid;
  mock_user.sid = sid;
  mock_user.auth_error = auth_error;
  mock_user.error_url = error_url;
  mock_user.captcha_token = captcha_token;
  mock_user.captcha_url = captcha_url;
  AddMockUser(mock_user);
}

// A convenience method to add a mock user to internal list of users.
void MockGaiaAuthenticator::AddMockUser(string email, string passwd,
                                        string auth_token, string lsid,
                                        string sid,
                                        enum gaia::AuthenticationError
                                        auth_error) {
  MockUser mock_user;
  mock_user.email = email;
  mock_user.passwd = passwd;
  mock_user.auth_token = auth_token;
  mock_user.lsid = lsid;
  mock_user.sid = sid;
  mock_user.auth_error = auth_error;
  AddMockUser(mock_user);
}

void MockGaiaAuthenticator::RemoveMockUser(const char* email) {
  mock_credentials_.erase(email);
}

void MockGaiaAuthenticator::RemoveAllMockUsers() {
  mock_credentials_.clear();
}

bool MockGaiaAuthenticator::Authenticate() {
  if (!should_save_credentials_) {
    ResetCredentials();
    return false;
  }
  return Authenticate(mock_credentials_[current_user_].email.c_str(),
                      mock_credentials_[current_user_].passwd.c_str(), true);
}

bool MockGaiaAuthenticator::Authenticate(const char* email,
                                         const char* passwd,
                                         bool should_save_credentials) {
  // Simply assign value to field; the value is read by the accessors when
  // reading.
  should_save_credentials_ = should_save_credentials;

  // Check if we already know about this mock user.
  if (mock_credentials_.find(email) == mock_credentials_.end()) {
    current_user_ = "";
    return false;
  }

  // If found, keep the current logged-in user available for token requests.
  current_user_ = email;

  // Finding a user does not necessarily imply that the user was logged in OK.
  // Therefore also check if the AuthenticationError is None.
  return (mock_credentials_[current_user_].auth_error == gaia::None);
}

// Remove any stored knowledge about the currently logged-in user, but keep
// details of mock users.
void MockGaiaAuthenticator::ResetCredentials() {
  current_user_ = "";
}

std::string MockGaiaAuthenticator::email() {
  return (current_user_.empty() || !should_save_credentials_) ? "" :
      mock_credentials_[current_user_].email;
}

std::string MockGaiaAuthenticator::auth_token() {
  return (current_user_.empty()) ? "" :
      mock_credentials_[current_user_].auth_token;
}

std::string MockGaiaAuthenticator::sid() {
  return (current_user_.empty()) ? "" :
      mock_credentials_[current_user_].sid;
}

std::string MockGaiaAuthenticator::lsid() {
  return (current_user_.empty()) ? "" :
      mock_credentials_[current_user_].lsid;
}

gaia::AuthenticationError MockGaiaAuthenticator::auth_error() {
  return (current_user_.empty()) ? gaia::CredentialsNotSet :
      mock_credentials_[current_user_].auth_error;
}

std::string MockGaiaAuthenticator::auth_error_url() {
  return (current_user_.empty()) ? "" :
      mock_credentials_[current_user_].error_url;
}

std::string MockGaiaAuthenticator::captcha_token() {
  return (current_user_.empty()) ? "" :
      mock_credentials_[current_user_].captcha_token;
}

std::string MockGaiaAuthenticator::captcha_url() {
  return (current_user_.empty()) ? "" :
      mock_credentials_[current_user_].captcha_url;
}

}  // namespace browser_sync
