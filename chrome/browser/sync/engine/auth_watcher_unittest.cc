// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE entry.

#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/test/test_file_util.h"
#include "base/waitable_event.h"
#include "chrome/browser/sync/engine/all_status.h"
#include "chrome/browser/sync/engine/auth_watcher.h"
#include "chrome/browser/sync/engine/net/gaia_authenticator.h"
#include "chrome/browser/sync/engine/net/http_return.h"
#include "chrome/browser/sync/notifier/listener/talk_mediator_impl.h"
#include "chrome/browser/sync/util/user_settings.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "chrome/test/sync/engine/mock_server_connection.h"
#include "chrome/test/sync/engine/test_directory_setter_upper.h"
#include "testing/gtest/include/gtest/gtest.h"

static FilePath::CharType kUserSettingsDB[] =
  FILE_PATH_LITERAL("Settings.sqlite3");
static const char* kTestUserAgent = "useragent";
static const char* kTestServiceId = "serviceid";
static const char* kTestGaiaURL = "http://gaia_url";
static const char* kUserDisplayName = "Mr. Auth Watcher";
static const char* kUserDisplayEmail = "authwatcherdisplay@gmail.com";
static const char* kTestEmail = "authwatchertest@gmail.com";
static const char* kWrongPassword = "wrongpassword";
static const char* kCorrectPassword = "correctpassword";
static const char* kValidSID = "validSID";
static const char* kValidLSID = "validLSID";
static const char* kInvalidAuthToken = "invalidAuthToken";
static const char* kValidAuthToken = "validAuthToken";

namespace browser_sync {

class GaiaAuthMockForAuthWatcher : public browser_sync::GaiaAuthenticator {
 public:
  GaiaAuthMockForAuthWatcher() : browser_sync::GaiaAuthenticator(
      kTestUserAgent, kTestServiceId, kTestGaiaURL),
      use_bad_auth_token_(false) {}
  virtual ~GaiaAuthMockForAuthWatcher() {}

  void SendBadAuthTokenForNextRequest() { use_bad_auth_token_ = true; }

 protected:
  bool PerformGaiaRequest(const AuthParams& params, AuthResults* results) {
    if (params.password == kWrongPassword) {
      results->auth_error = browser_sync::BadAuthentication;
      return false;
    }
    if (params.password == kCorrectPassword) {
      results->sid = kValidSID;
      results->lsid = kValidLSID;
      results->auth_token = kValidAuthToken;
    }
    if (use_bad_auth_token_) {
      results->auth_token = kInvalidAuthToken;
      use_bad_auth_token_ = false;
    }
    return true;
  }

  bool LookupEmail(AuthResults* results) {
    results->signin = GMAIL_SIGNIN;
    return true;
  }

 private:
  // Whether we should send an invalid auth token on the next request.
  bool use_bad_auth_token_;
};

class AuthWatcherTest : public testing::Test {
 public:
  AuthWatcherTest() : metadb_(kUserDisplayEmail),
                      consumer_ready(false, false),
                      event_produced(false, false) {}
  virtual void SetUp() {
    metadb_.SetUp();
    connection_.reset(new MockConnectionManager(metadb_.manager(),
                                                metadb_.name()));
    // Mock out data that would normally be sent back from a server.
    connection()->SetAuthenticationResponseInfo(kValidAuthToken,
        kUserDisplayName, kUserDisplayEmail, "ID");
    allstatus_.reset(new AllStatus());
    user_settings_.reset(new UserSettings());
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    FilePath user_settings_path = temp_dir_.path().Append(kUserSettingsDB);
    user_settings_->Init(user_settings_path);
    gaia_auth_ = new GaiaAuthMockForAuthWatcher();
    talk_mediator_.reset(new TalkMediatorImpl());
    auth_watcher_ = new AuthWatcher(metadb_.manager(), connection_.get(),
        allstatus_.get(), kTestUserAgent, kTestServiceId, kTestGaiaURL,
        user_settings_.get(), gaia_auth_, talk_mediator_.get());
    authwatcher_hookup_.reset(NewEventListenerHookup(auth_watcher_->channel(),
        this, &AuthWatcherTest::HandleAuthWatcherEvent));
  }

  virtual void TearDown() {
    metadb_.TearDown();
    auth_watcher_->Shutdown();
    EXPECT_FALSE(auth_watcher()->message_loop());
  }

  void HandleAuthWatcherEvent(const AuthWatcherEvent& event) {
    if (event.what_happened == AuthWatcherEvent::AUTHWATCHER_DESTROYED)
      return;
    consumer_ready.Wait();  // Block progress until the test is ready.

    last_event_reason_ = event.what_happened;
    if (event.what_happened == AuthWatcherEvent::AUTH_SUCCEEDED)
      user_email_ = event.user_email;

    event_produced.Signal();
  }

  AuthWatcherEvent::WhatHappened ConsumeNextEvent() {
    consumer_ready.Signal();
    event_produced.Wait();
    return last_event_reason_;
  }

  AuthWatcher* auth_watcher() { return auth_watcher_.get(); }
  MockConnectionManager* connection() { return connection_.get(); }
  GaiaAuthMockForAuthWatcher* gaia_auth() { return gaia_auth_; }
  const std::string& user_email() { return user_email_; }

 private:
  // Responsible for creating / deleting a temp dir containing user settings DB.
  ScopedTempDir temp_dir_;

  // The event listener hookup registered for HandleAuthWatcherEvent.
  scoped_ptr<EventListenerHookup> authwatcher_hookup_;

  // The sync engine pieces necessary to run an AuthWatcher.
  TriggeredOpenTestDirectorySetterUpper metadb_;
  scoped_ptr<MockConnectionManager> connection_;
  scoped_ptr<AllStatus> allstatus_;
  scoped_ptr<UserSettings> user_settings_;
  GaiaAuthMockForAuthWatcher* gaia_auth_;  // Owned by auth_watcher_.
  scoped_ptr<TalkMediator> talk_mediator_;
  scoped_refptr<AuthWatcher> auth_watcher_;

  // This is used to block the AuthWatcherThread when it raises events until we
  // are ready to read the event.  It is not a manual-reset event, so it goes
  // straight back to non-signaled after one thread (the main thread) is
  // signaled (or "consumes" the signaled state).
  base::WaitableEvent consumer_ready;

  // This is signaled by the AuthWatcherThread after it sets last_event_reason_
  // and possibly user_email_ for us to read.
  base::WaitableEvent event_produced;

  // The 'WhatHappened' value from the last AuthWatcherEvent we handled.
  AuthWatcherEvent::WhatHappened last_event_reason_;

  // Set when we receive an AUTH_SUCCEEDED event.
  std::string user_email_;

  DISALLOW_COPY_AND_ASSIGN(AuthWatcherTest);
};

TEST_F(AuthWatcherTest, Construction) {
  EXPECT_TRUE(auth_watcher()->message_loop());
  EXPECT_EQ("SyncEngine_AuthWatcherThread",
            auth_watcher()->message_loop()->thread_name());
  EXPECT_TRUE(auth_watcher()->auth_backend_thread_.IsRunning());
  EXPECT_EQ(AuthWatcher::NOT_AUTHENTICATED, auth_watcher()->status());
}

TEST_F(AuthWatcherTest, AuthenticateGaiaAuthFailure) {
  auth_watcher()->Authenticate(kTestEmail, kWrongPassword,
      std::string(),  // captcha_token
      std::string(),  // captcha_value
      false);  // persist_creds_to_disk
  EXPECT_EQ(AuthWatcherEvent::AUTHENTICATION_ATTEMPT_START, ConsumeNextEvent());
  EXPECT_EQ(AuthWatcherEvent::GAIA_AUTH_FAILED, ConsumeNextEvent());
}

TEST_F(AuthWatcherTest, AuthenticateBadAuthToken) {
  gaia_auth()->SendBadAuthTokenForNextRequest();
  auth_watcher()->Authenticate(kTestEmail, kCorrectPassword, std::string(),
      std::string(), false);
  EXPECT_EQ(AuthWatcherEvent::AUTHENTICATION_ATTEMPT_START, ConsumeNextEvent());
  EXPECT_EQ(AuthWatcherEvent::SERVICE_AUTH_FAILED, ConsumeNextEvent());
}

TEST_F(AuthWatcherTest, AuthenticateSuccess) {
  auth_watcher()->Authenticate(kTestEmail, kCorrectPassword, std::string(),
      std::string(), false);
  EXPECT_EQ(AuthWatcherEvent::AUTHENTICATION_ATTEMPT_START, ConsumeNextEvent());
  EXPECT_EQ(AuthWatcherEvent::AUTH_SUCCEEDED, ConsumeNextEvent());

  // The server responds with a different email than what we used in the call
  // to Authenticate, and the AuthWatcher should have told us about.
  EXPECT_EQ(kUserDisplayEmail, user_email());
}

TEST_F(AuthWatcherTest, AuthenticateWithTokenBadAuthToken) {
  auth_watcher()->AuthenticateWithToken(kTestEmail, kInvalidAuthToken);
  EXPECT_EQ(AuthWatcherEvent::SERVICE_AUTH_FAILED, ConsumeNextEvent());
}

TEST_F(AuthWatcherTest, AuthenticateWithTokenSuccess) {
  auth_watcher()->AuthenticateWithToken(kTestEmail, kValidAuthToken);
  EXPECT_EQ(AuthWatcherEvent::AUTH_SUCCEEDED, ConsumeNextEvent());
  EXPECT_EQ(kUserDisplayEmail, user_email());
}

}  // namespace browser_sync
