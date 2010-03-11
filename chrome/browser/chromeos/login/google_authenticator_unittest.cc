// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/google_authenticator.h"

#include <errno.h>
#include <string>
#include <vector>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chrome_thread.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::_;

class MockConsumer : public LoginStatusConsumer {
 public:
  MockConsumer() {}
  ~MockConsumer() {}
  MOCK_METHOD1(OnLoginFailure, void(const std::string error));
  MOCK_METHOD1(OnLoginSuccess, void(const std::string username));
};

class GoogleAuthenticatorTest : public ::testing::Test {
 public:
  GoogleAuthenticatorTest() {
    memset(fake_hash_, 0, sizeof(fake_hash_));
    fake_hash_[0] = 10;
    fake_hash_[1] = 1;
    fake_hash_[7] = 10 << 4;
    hash_ascii_.assign("0a010000000000a0");
    hash_ascii_.append(std::string(16,'0'));
  }
  ~GoogleAuthenticatorTest() {}

  unsigned char fake_hash_[32];
  std::string hash_ascii_;
  std::string username_;
  std::string error_;
};

TEST_F(GoogleAuthenticatorTest, SaltToAsciiTest) {
  unsigned char fake_salt[8] = { 0 };
  fake_salt[0] = 10;
  fake_salt[1] = 1;
  fake_salt[7] = 10 << 4;
  std::vector<unsigned char> salt_v(fake_salt, fake_salt + sizeof(fake_salt));

  MockCryptohomeLibrary library;
  GoogleAuthenticator auth(NULL, &library);
  auth.set_system_salt(salt_v);

  EXPECT_EQ("0a010000000000a0", auth.SaltAsAscii());
}

TEST_F(GoogleAuthenticatorTest, OnLoginSuccessTest) {
  error_.assign("Unexpected error");

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_));

  MockCryptohomeLibrary library;
  EXPECT_CALL(library, Mount(username_, hash_ascii_))
      .WillOnce(Return(true));

  GoogleAuthenticator auth(&consumer, &library);
  auth.OnLoginSuccess(&consumer, &library, username_, hash_ascii_, error_);
}

TEST_F(GoogleAuthenticatorTest, MountFailureTest) {
  error_.assign("Expected error");

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(error_));

  MockCryptohomeLibrary library;
  EXPECT_CALL(library, Mount(username_, hash_ascii_))
      .WillOnce(Return(false));

  GoogleAuthenticator auth(&consumer, &library);
  auth.OnLoginSuccess(&consumer, &library, username_, hash_ascii_, error_);
}

static void Quit() { MessageLoop::current()->Quit(); }
TEST_F(GoogleAuthenticatorTest, LoginNetFailureTest) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  int error_no = ECONNRESET;
  error_.assign(strerror(error_no));
  GURL source;
  ResponseCookies cookies;

  URLRequestStatus status(URLRequestStatus::FAILED, error_no);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(error_))
      .WillOnce(InvokeWithoutArgs(Quit));
  MockCryptohomeLibrary library;
  EXPECT_CALL(library, CheckKey(username_, hash_ascii_))
      .WillOnce(Return(false));

  GoogleAuthenticator auth(&consumer, &library);
  auth.set_password_hash(hash_ascii_);
  auth.OnURLFetchComplete(NULL, source, status, 0, cookies, std::string());
  MessageLoop::current()->Run();  // So tasks can be posted.
}

TEST_F(GoogleAuthenticatorTest, LoginDeniedTest) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  error_.assign("Error: NO!");
  GURL source;
  ResponseCookies cookies;

  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(error_))
      .WillOnce(InvokeWithoutArgs(Quit));
  MockCryptohomeLibrary library;

  GoogleAuthenticator auth(&consumer, &library);
  auth.OnURLFetchComplete(NULL, source, status, 403, cookies, error_);
  MessageLoop::current()->Run();  // So tasks can be posted.
}

TEST_F(GoogleAuthenticatorTest, OfflineLoginTest) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  int error_no = ECONNRESET;
  error_.assign(strerror(error_no));
  GURL source;
  ResponseCookies cookies;

  URLRequestStatus status(URLRequestStatus::FAILED, error_no);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_))
      .WillOnce(InvokeWithoutArgs(Quit));
  MockCryptohomeLibrary library;
  EXPECT_CALL(library, CheckKey(username_, hash_ascii_))
      .WillOnce(Return(true));
  EXPECT_CALL(library, Mount(username_, hash_ascii_))
      .WillOnce(Return(true));

  GoogleAuthenticator auth(&consumer, &library);
  auth.set_password_hash(hash_ascii_);
  auth.OnURLFetchComplete(NULL, source, status, 0, cookies, std::string());
  MessageLoop::current()->Run();  // So tasks can be posted.
}

TEST_F(GoogleAuthenticatorTest, OnlineLoginTest) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  GURL source;
  ResponseCookies cookies;
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_))
      .WillOnce(InvokeWithoutArgs(Quit));
  MockCryptohomeLibrary library;
  EXPECT_CALL(library, Mount(username_, hash_ascii_))
      .WillOnce(Return(true));

  GoogleAuthenticator auth(&consumer, &library);
  auth.set_password_hash(hash_ascii_);
  auth.OnURLFetchComplete(NULL, source, status, 200, cookies, std::string());
  MessageLoop::current()->Run();  // So tasks can be posted.
}

}  // namespace chromeos
