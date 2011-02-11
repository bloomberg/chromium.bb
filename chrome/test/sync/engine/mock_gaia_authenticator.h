// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Use MockGaiaAuthenticator to test your application by faking a login session.
// This mock object should be initialized with the response you expect it to
// return for multiple users, and then can be used in exactly the same way
// as the real GaiaAuthenticator.
//
// Sample usage:
// MockGaiaAuthenticator mock_gaia_auth("User-Agent", SYNC_SERVICE_NAME,
//     "any random string");
// mock_gaia_auth.AddMockUser("email", "password", "authtoken", "lsid", "sid",
//                            AuthenticationError);
// mock_gaia_auth.AddMockUser("email2", "password2", "authtoken2", "lsid2",
//                            "sid2", AuthenticationError, error_url,
//                            "captcha_token", "captcha_url");
// if (gaia_auth.Authenticate("email", "passwd")) {
//   // Do something with: gaia_auth.auth_token(), or gaia_auth.sid(),
//   // or gaia_auth.lsid()
// }

#ifndef CHROME_TEST_SYNC_ENGINE_MOCK_GAIA_AUTHENTICATOR_H_
#define CHROME_TEST_SYNC_ENGINE_MOCK_GAIA_AUTHENTICATOR_H_
#pragma once

#include <map>
#include <string>

#include "base/port.h"

#include "base/basictypes.h"
#include "chrome/common/net/gaia/gaia_authenticator.h"

namespace browser_sync {

// A struct used internally for storing a user's credentials. You can either
// create one yourself, or use the convenience methods to have the
// MockGaiaAuthenticator create one for you.
typedef struct {
  std::string email;
  std::string passwd;
  std::string auth_token;
  std::string sid;
  std::string lsid;
  gaia::AuthenticationError auth_error;
  std::string captcha_token;
  std::string captcha_url;
  std::string error_url;
} MockUser;

// MockGaiaAuthenticator can be used to fake Gaia authentication without
// actually making a network connection. For details about the methods shared
// with GaiaAuthenticator, see GaiaAuthenticator in gaia_auth.h. Only methods
// that are unique to MockGaiaAuthenticator are documented in this file.
class MockGaiaAuthenticator {
 public:
  MockGaiaAuthenticator(const char* user_agent, const char* service_id,
                        const char* gaia_url);
  ~MockGaiaAuthenticator();

  // Add a mock user; takes a struct. You can populate any or all fields when
  // adding a user. The email field is required, all others optional.
  void AddMockUser(MockUser mock_user);

  // A convenience method that makes it easy to create new mock users in a
  // single method call. Includes all parameters.
  void AddMockUser(std::string email, std::string passwd,
                   std::string auth_token,
                   std::string lsid, std::string sid,
                   gaia::AuthenticationError auth_error,
                   std::string error_url, std::string captcha_token,
                   std::string captcha_url);

  // A convenience method that makes it easy to create new mock users in a
  // single method call. Includes only the most common parameters. See overload
  // if you want to pass all parameters.
  void AddMockUser(std::string email, std::string passwd,
                   std::string auth_token,
                   std::string lsid, std::string sid,
                   enum gaia::AuthenticationError auth_error);

  // Removes a mock user from the current list of added users.
  void RemoveMockUser(const char* email);

  // Removes all mock users from the current list of added users.
  void RemoveAllMockUsers();

  // See GaiaAuthenticator::Authenticate()
  bool Authenticate();

  // See GaiaAuthenticator::Authenticate(...)
  bool Authenticate(const char* user_name, const char* password,
                    bool should_save_credentials = false);

  // See GaiaAuthenticator::Authenticate(...)
  void ResetCredentials();

  // Accessors follow.
  std::string email();
  std::string auth_token();
  std::string sid();
  std::string lsid();
  gaia::AuthenticationError auth_error();
  std::string auth_error_url();
  std::string captcha_token();
  std::string captcha_url();

 private:
  bool should_save_credentials_;
  std::map<std::string, MockUser> mock_credentials_;
  std::string current_user_;
};

}  // namespace browser_sync

#endif  // CHROME_TEST_SYNC_ENGINE_MOCK_GAIA_AUTHENTICATOR_H_
