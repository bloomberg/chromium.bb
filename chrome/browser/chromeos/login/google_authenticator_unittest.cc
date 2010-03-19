// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/google_authenticator.h"
#include "chrome/browser/chromeos/login/client_login_response_handler.h"
#include "chrome/browser/chromeos/login/issue_response_handler.h"

#include <errno.h>
#include <string>
#include <vector>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/login/mock_auth_response_handler.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/url_fetcher.h"
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
  MOCK_METHOD2(OnLoginSuccess, void(const std::string username,
                                    const std::vector<std::string> cookies));
};

class GoogleAuthenticatorTest : public ::testing::Test {
 public:
  GoogleAuthenticatorTest() : username_("me@nowhere.org") {
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
  ResponseCookies cookies_;
};

TEST_F(GoogleAuthenticatorTest, SaltToAsciiTest) {
  unsigned char fake_salt[8] = { 0 };
  fake_salt[0] = 10;
  fake_salt[1] = 1;
  fake_salt[7] = 10 << 4;
  std::vector<unsigned char> salt_v(fake_salt, fake_salt + sizeof(fake_salt));

  MockCryptohomeLibrary library;
  GoogleAuthenticator auth(NULL, &library, NULL, NULL);
  auth.set_system_salt(salt_v);

  EXPECT_EQ("0a010000000000a0", auth.SaltAsAscii());
}

TEST_F(GoogleAuthenticatorTest, ClientLoginResponseHandlerTest) {
  ClientLoginResponseHandler handler(NULL);
  std::string input("a\nb\n");
  std::string expected("a&b&");
  expected.append(ClientLoginResponseHandler::kService);

  scoped_ptr<URLFetcher> fetcher(handler.Handle(input, NULL));
  EXPECT_EQ(expected, handler.payload());
}

TEST_F(GoogleAuthenticatorTest, IssueResponseHandlerTest) {
  IssueResponseHandler handler(NULL);
  std::string input("a\n");
  std::string expected(IssueResponseHandler::kTokenAuthUrl);
  expected.append(input);

  scoped_ptr<URLFetcher> fetcher(handler.Handle(input, NULL));
  EXPECT_EQ(expected, handler.token_url());
}

TEST_F(GoogleAuthenticatorTest, OnLoginSuccessTest) {
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, _));

  MockCryptohomeLibrary library;
  EXPECT_CALL(library, Mount(username_, hash_ascii_))
      .WillOnce(Return(true));

  GoogleAuthenticator auth(&consumer, &library, NULL, NULL);
  auth.OnLoginSuccess(&consumer, &library, username_, hash_ascii_, cookies_);
}

TEST_F(GoogleAuthenticatorTest, MountFailureTest) {
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(_));

  MockCryptohomeLibrary library;
  EXPECT_CALL(library, Mount(username_, hash_ascii_))
      .WillOnce(Return(false));

  GoogleAuthenticator auth(&consumer, &library, NULL, NULL);
  auth.OnLoginSuccess(&consumer, &library, username_, hash_ascii_, cookies_);
}

static void Quit() { MessageLoop::current()->Quit(); }
TEST_F(GoogleAuthenticatorTest, LoginNetFailureTest) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  int error_no = ECONNRESET;
  std::string data(strerror(error_no));
  GURL source;

  URLRequestStatus status(URLRequestStatus::FAILED, error_no);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(data))
      .WillOnce(InvokeWithoutArgs(Quit));
  MockCryptohomeLibrary library;
  EXPECT_CALL(library, CheckKey(username_, hash_ascii_))
      .WillOnce(Return(false));

  GoogleAuthenticator auth(&consumer, &library, NULL, NULL);
  auth.set_password_hash(hash_ascii_);
  auth.set_username(username_);
  auth.OnURLFetchComplete(NULL, source, status, 0, cookies_, data);
  MessageLoop::current()->Run();  // So tasks can be posted.
}

TEST_F(GoogleAuthenticatorTest, LoginDeniedTest) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  std::string data("Error: NO!");
  GURL source(AuthResponseHandler::kTokenAuthUrl);

  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(data))
      .WillOnce(InvokeWithoutArgs(Quit));
  MockCryptohomeLibrary library;

  GoogleAuthenticator auth(&consumer, &library, NULL, NULL);
  auth.OnURLFetchComplete(NULL, source, status, 403, cookies_, data);
  MessageLoop::current()->Run();  // So tasks can be posted.
}

TEST_F(GoogleAuthenticatorTest, OfflineLoginTest) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  int error_no = ECONNRESET;
  std::string data(strerror(error_no));
  GURL source;

  URLRequestStatus status(URLRequestStatus::FAILED, error_no);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, cookies_))
      .WillOnce(InvokeWithoutArgs(Quit));
  MockCryptohomeLibrary library;
  EXPECT_CALL(library, CheckKey(username_, hash_ascii_))
      .WillOnce(Return(true));
  EXPECT_CALL(library, Mount(username_, hash_ascii_))
      .WillOnce(Return(true));

  GoogleAuthenticator auth(&consumer, &library, NULL, NULL);
  auth.set_password_hash(hash_ascii_);
  auth.set_username(username_);
  auth.OnURLFetchComplete(NULL, source, status, 0, cookies_, data);
  MessageLoop::current()->Run();  // So tasks can be posted.
}

TEST_F(GoogleAuthenticatorTest, ClientLoginPassIssueAuthTokenFailTest) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  std::string data("Error: NO!");
  GURL cl_source(AuthResponseHandler::kClientLoginUrl);
  GURL iat_source(AuthResponseHandler::kIssueAuthTokenUrl);
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(data))
      .WillOnce(InvokeWithoutArgs(Quit));
  MockCryptohomeLibrary library;
  MockAuthResponseHandler* cl_handler = new MockAuthResponseHandler;
  EXPECT_CALL(*cl_handler, CanHandle(cl_source))
      .WillOnce(Return(true));

  GoogleAuthenticator auth(&consumer,
                           &library,
                           cl_handler,  // takes ownership.
                           new IssueResponseHandler(NULL));
  auth.set_password_hash(hash_ascii_);
  auth.set_username(username_);

  EXPECT_CALL(*cl_handler, Handle(_, &auth))
      .WillOnce(Return(new URLFetcher(GURL(""),
                                      URLFetcher::POST,
                                      &auth)));

  auth.OnURLFetchComplete(NULL,
                          cl_source,
                          status,
                          200,
                          cookies_,
                          std::string());
  auth.OnURLFetchComplete(NULL, iat_source, status, 403, cookies_, data);
  MessageLoop::current()->Run();  // So tasks can be posted.
}

TEST_F(GoogleAuthenticatorTest, OnlineLoginTest) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  GURL source(AuthResponseHandler::kTokenAuthUrl);
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, cookies_))
      .WillOnce(InvokeWithoutArgs(Quit));
  MockCryptohomeLibrary library;
  EXPECT_CALL(library, Mount(username_, hash_ascii_))
      .WillOnce(Return(true));

  GoogleAuthenticator auth(&consumer,
                           &library,
                           new ClientLoginResponseHandler(NULL),
                           new IssueResponseHandler(NULL));
  auth.set_password_hash(hash_ascii_);
  auth.set_username(username_);
  auth.OnURLFetchComplete(NULL, source, status, 200, cookies_, std::string());
  MessageLoop::current()->Run();  // So tasks can be posted.
}

}  // namespace chromeos
