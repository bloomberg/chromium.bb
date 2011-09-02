// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for MockGaiaAuthenticator.

#include "base/basictypes.h"
#include "base/port.h"
#include "chrome/browser/sync/protocol/service_constants.h"
#include "chrome/browser/sync/test/engine/mock_gaia_authenticator.h"
#include "chrome/common/net/gaia/gaia_authenticator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Test if authentication succeeds for a mock user added earlier.
TEST(MockGaiaAuthenticatorTest, TestAuthenticationSuccess) {
  browser_sync::MockGaiaAuthenticator
    mock_gaia_auth("User-Agent", SYNC_SERVICE_NAME,
                   "some random url");

  // Initialize a mock user, and add to mock authenticator.
  browser_sync::MockUser mock_user;
  mock_user.email = "test";
  mock_user.passwd = "passwd";
  mock_user.auth_token = "SomeAuthToken";
  mock_user.lsid = "SomeLSID";
  mock_user.sid = "SomeSID";
  mock_user.auth_error = gaia::None;
  mock_gaia_auth.AddMockUser(mock_user);

  // Assert away ...
  ASSERT_TRUE(mock_gaia_auth.Authenticate("test", "passwd"));
  ASSERT_STREQ(mock_gaia_auth.auth_token().c_str(), "SomeAuthToken");
  ASSERT_STREQ(mock_gaia_auth.sid().c_str(), "SomeSID");
  ASSERT_STREQ(mock_gaia_auth.lsid().c_str(), "SomeLSID");
}

// Test if authentication fails for a mock user that was never added.
TEST(MockGaiaAuthenticatorTest, TestAuthenticationFailure) {
  browser_sync::MockGaiaAuthenticator
    mock_gaia_auth("User-Agent", SYNC_SERVICE_NAME,
                   "some random url");

  // At this point, in real code, we would be adding mock users to our mock
  // object. However, in this unittest, we exercise the path where this step is
  // missing, and assert that the outcome is still consistent with that of the
  // real GaiaAuthenticator.

  // Assert away ...
  ASSERT_FALSE(mock_gaia_auth.Authenticate("test", "passwd"));
  ASSERT_STREQ(mock_gaia_auth.auth_token().c_str(), "");
  ASSERT_STREQ(mock_gaia_auth.sid().c_str(), "");
  ASSERT_STREQ(mock_gaia_auth.lsid().c_str(), "");
}

// Test if authentication fails after a mock user is removed.
TEST(MockGaiaAuthenticatorTest, TestRemoveMockUser) {
  // Instantiate authenticator.
  browser_sync::MockGaiaAuthenticator
    mock_gaia_auth("User-Agent", SYNC_SERVICE_NAME,
                   "some random url");

  // Add our mock user
  mock_gaia_auth.AddMockUser("test", "passwd", "SomeAuthToken", "SomeLSID",
      "SomeSID", gaia::None);

  // Make sure authentication succeeds.
  ASSERT_TRUE(mock_gaia_auth.Authenticate("test", "passwd"));
  ASSERT_STREQ(mock_gaia_auth.auth_token().c_str(), "SomeAuthToken");
  ASSERT_STREQ(mock_gaia_auth.sid().c_str(), "SomeSID");
  ASSERT_STREQ(mock_gaia_auth.lsid().c_str(), "SomeLSID");

  // Remove the just-added user from our list.
  mock_gaia_auth.RemoveMockUser("test");

  // ... and authentication should fail.
  ASSERT_FALSE(mock_gaia_auth.Authenticate("test", "passwd"));
  ASSERT_STREQ(mock_gaia_auth.auth_token().c_str(), "");
  ASSERT_STREQ(mock_gaia_auth.sid().c_str(), "");
  ASSERT_STREQ(mock_gaia_auth.lsid().c_str(), "");
}

// Test if authentication fails after all mock users are removed.
TEST(MockGaiaAuthenticatorTest, TestRemoveAllMockUsers) {
  // Instantiate authenticator.
  browser_sync::MockGaiaAuthenticator
    mock_gaia_auth("User-Agent", SYNC_SERVICE_NAME,
                   "some random url");

  // Add our sample mock user.
  mock_gaia_auth.AddMockUser("test", "passwd", "SomeAuthToken", "SomeLSID",
      "SomeSID", gaia::None);

  // Make sure authentication succeeds
  ASSERT_TRUE(mock_gaia_auth.Authenticate("test", "passwd"));
  ASSERT_STREQ(mock_gaia_auth.auth_token().c_str(), "SomeAuthToken");
  ASSERT_STREQ(mock_gaia_auth.sid().c_str(), "SomeSID");
  ASSERT_STREQ(mock_gaia_auth.lsid().c_str(), "SomeLSID");

  // Now remove all mock users.
  mock_gaia_auth.RemoveAllMockUsers();

  // And confirm that authentication fails.
  ASSERT_FALSE(mock_gaia_auth.Authenticate("test", "passwd"));
  ASSERT_STREQ(mock_gaia_auth.auth_token().c_str(), "");
  ASSERT_STREQ(mock_gaia_auth.sid().c_str(), "");
  ASSERT_STREQ(mock_gaia_auth.lsid().c_str(), "");
}

// Test authentication with saved credentials.
TEST(MockGaiaAuthenticatorTest, TestSavedCredentials) {
  // Instantiate authenticator.
  browser_sync::MockGaiaAuthenticator
    mock_gaia_auth("User-Agent", SYNC_SERVICE_NAME,
                   "some random url");

  // Add our sample mock user.
  mock_gaia_auth.AddMockUser("test", "passwd", "SomeAuthToken", "SomeLSID",
      "SomeSID", gaia::None);

  // Ask to save credentials.
  ASSERT_TRUE(mock_gaia_auth.Authenticate("test", "passwd", true));
  ASSERT_STREQ(mock_gaia_auth.auth_token().c_str(), "SomeAuthToken");
  ASSERT_STREQ(mock_gaia_auth.sid().c_str(), "SomeSID");
  ASSERT_STREQ(mock_gaia_auth.lsid().c_str(), "SomeLSID");

  // Now make a call that uses saved credentials, and assert that we get the
  // same tokens back.
  ASSERT_TRUE(mock_gaia_auth.Authenticate());
  ASSERT_STREQ(mock_gaia_auth.email().c_str(), "test");
  ASSERT_STREQ(mock_gaia_auth.auth_token().c_str(), "SomeAuthToken");
  ASSERT_STREQ(mock_gaia_auth.sid().c_str(), "SomeSID");
  ASSERT_STREQ(mock_gaia_auth.lsid().c_str(), "SomeLSID");

  // Now clear the saved credentials by toggling the flag while authenticating.
  ASSERT_TRUE(mock_gaia_auth.Authenticate("test", "passwd", false));

  // Test if saved credentials have been cleared.
  ASSERT_STREQ(mock_gaia_auth.email().c_str(), "");

  // Assert that current authentication session still succeeds (we only asked
  // not to save it for future requests.)
  ASSERT_STREQ(mock_gaia_auth.auth_token().c_str(), "SomeAuthToken");
  ASSERT_STREQ(mock_gaia_auth.sid().c_str(), "SomeSID");
  ASSERT_STREQ(mock_gaia_auth.lsid().c_str(), "SomeLSID");

  // Now try to use saved credentials:
  ASSERT_STREQ(mock_gaia_auth.email().c_str(), "");
  ASSERT_FALSE(mock_gaia_auth.Authenticate());

  // And assert that any future requests that rely on saved credentials fail.
  ASSERT_STREQ(mock_gaia_auth.auth_token().c_str(), "");
  ASSERT_STREQ(mock_gaia_auth.sid().c_str(), "");
  ASSERT_STREQ(mock_gaia_auth.lsid().c_str(), "");
}

}  // namespace
