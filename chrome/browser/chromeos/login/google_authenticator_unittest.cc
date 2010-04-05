// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/login/client_login_response_handler.h"
#include "chrome/browser/chromeos/login/google_authenticator.h"
#include "chrome/browser/chromeos/login/issue_response_handler.h"
#include "chrome/browser/chromeos/login/mock_auth_response_handler.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using namespace file_util;
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::_;

namespace chromeos {

class MockConsumer : public LoginStatusConsumer {
 public:
  MockConsumer() {}
  ~MockConsumer() {}
  MOCK_METHOD1(OnLoginFailure, void(const std::string& error));
  MOCK_METHOD2(OnLoginSuccess, void(const std::string& username,
                                    const std::string& data));
};

class GoogleAuthenticatorTest : public ::testing::Test {
 public:
  GoogleAuthenticatorTest()
      : username_("me@nowhere.org"),
        bytes_as_ascii_("ffff") {
    memset(fake_hash_, 0, sizeof(fake_hash_));
    fake_hash_[0] = 10;
    fake_hash_[1] = 1;
    fake_hash_[7] = 10 << 4;
    hash_ascii_.assign("0a010000000000a0");
    hash_ascii_.append(std::string(16, '0'));

    memset(raw_bytes_, 0xff, sizeof(raw_bytes_));
  }
  ~GoogleAuthenticatorTest() {}

  virtual void SetUp() {
    chromeos::CrosLibrary::TestApi* test_api =
        chromeos::CrosLibrary::Get()->GetTestApi();

    loader_ = new MockLibraryLoader();
    ON_CALL(*loader_, Load(_))
        .WillByDefault(Return(true));
    EXPECT_CALL(*loader_, Load(_))
        .Times(AnyNumber());

    test_api->SetLibraryLoader(loader_);

    mock_library_ = new MockCryptohomeLibrary();
    test_api->SetCryptohomeLibrary(mock_library_);
}

  // Tears down the test fixture.
  virtual void TearDown() {
    // Prevent bogus gMock leak check from firing.
    chromeos::CrosLibrary::TestApi* test_api =
        chromeos::CrosLibrary::Get()->GetTestApi();
    test_api->SetLibraryLoader(NULL);
    test_api->SetCryptohomeLibrary(NULL);
  }

  FilePath PopulateTempFile(const char* data, int data_len) {
    FilePath out;
    FILE* tmp_file = CreateAndOpenTemporaryFile(&out);
    EXPECT_NE(tmp_file, reinterpret_cast<FILE*>(NULL));
    EXPECT_EQ(WriteFile(out, data, data_len), data_len);
    EXPECT_TRUE(CloseFile(tmp_file));
    return out;
  }

  FilePath FakeLocalaccountFile(const std::string& ascii) {
    FilePath exe_dir;
    FilePath local_account_file;
    PathService::Get(base::DIR_EXE, &exe_dir);
    FILE* tmp_file = CreateAndOpenTemporaryFileInDir(exe_dir,
                                                     &local_account_file);
    int ascii_len = ascii.length();
    EXPECT_NE(tmp_file, reinterpret_cast<FILE*>(NULL));
    EXPECT_EQ(WriteFile(local_account_file, ascii.c_str(), ascii_len),
              ascii_len);
    EXPECT_TRUE(CloseFile(tmp_file));
    return local_account_file;
  }

  unsigned char fake_hash_[32];
  std::string hash_ascii_;
  std::string username_;
  std::string data_;
  ResponseCookies cookies_;
  // Mocks, destroyed by CrosLibrary class.
  MockCryptohomeLibrary* mock_library_;
  MockLibraryLoader* loader_;
  char raw_bytes_[2];
  std::string bytes_as_ascii_;
};

TEST_F(GoogleAuthenticatorTest, SaltToAsciiTest) {
  unsigned char fake_salt[8] = { 0 };
  fake_salt[0] = 10;
  fake_salt[1] = 1;
  fake_salt[7] = 10 << 4;
  std::vector<unsigned char> salt_v(fake_salt, fake_salt + sizeof(fake_salt));

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(NULL));
  auth->set_system_salt(salt_v);

  EXPECT_EQ("0a010000000000a0", auth->SaltAsAscii());
}

TEST_F(GoogleAuthenticatorTest, ReadSaltTest) {
  FilePath tmp_file_path = PopulateTempFile(raw_bytes_, sizeof(raw_bytes_));

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(NULL));
  auth->LoadSystemSalt(tmp_file_path);
  EXPECT_EQ(auth->SaltAsAscii(), bytes_as_ascii_);
  Delete(tmp_file_path, false);
}

TEST_F(GoogleAuthenticatorTest, ReadLocalaccountTest) {
  FilePath tmp_file_path = FakeLocalaccountFile(bytes_as_ascii_);

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(NULL));
  auth->LoadLocalaccount(tmp_file_path.BaseName().value());
  EXPECT_EQ(auth->localaccount_, bytes_as_ascii_);
  Delete(tmp_file_path, false);
}

TEST_F(GoogleAuthenticatorTest, ReadLocalaccountTrailingWSTest) {
  FilePath tmp_file_path =
      FakeLocalaccountFile(StringPrintf("%s\n", bytes_as_ascii_.c_str()));

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(NULL));
  auth->LoadLocalaccount(tmp_file_path.BaseName().value());
  EXPECT_EQ(auth->localaccount_, bytes_as_ascii_);
  Delete(tmp_file_path, false);
}

TEST_F(GoogleAuthenticatorTest, ReadNoLocalaccountTest) {
  FilePath tmp_file_path = FakeLocalaccountFile(bytes_as_ascii_);
  EXPECT_TRUE(Delete(tmp_file_path, false));  // Ensure non-existent file.

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(NULL));
  auth->LoadLocalaccount(tmp_file_path.BaseName().value());
  EXPECT_EQ(auth->localaccount_, std::string());
}

TEST_F(GoogleAuthenticatorTest, OnLoginSuccessTest) {
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, _));

  EXPECT_CALL(*mock_library_, Mount(username_, hash_ascii_))
      .WillOnce(Return(true));

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  auth->set_password_hash(hash_ascii_);
  auth->set_username(username_);
  auth->OnLoginSuccess(data_);
}

TEST_F(GoogleAuthenticatorTest, MountFailureTest) {
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(_));

  EXPECT_CALL(*mock_library_, Mount(username_, hash_ascii_))
      .WillOnce(Return(false));

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  auth->set_password_hash(hash_ascii_);
  auth->set_username(username_);
  auth->OnLoginSuccess(data_);
}

TEST_F(GoogleAuthenticatorTest, LoginNetFailureTest) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  int error_no = ECONNRESET;
  std::string data(strerror(error_no));
  GURL source;

  URLRequestStatus status(URLRequestStatus::FAILED, error_no);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(data))
      .Times(1);
  EXPECT_CALL(*mock_library_, CheckKey(username_, hash_ascii_))
      .WillOnce(Return(false));

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  auth->set_password_hash(hash_ascii_);
  auth->set_username(username_);
  auth->OnURLFetchComplete(NULL, source, status, 0, cookies_, data);
  message_loop.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, LoginDeniedTest) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  std::string data("Error: NO!");
  GURL source(AuthResponseHandler::kTokenAuthUrl);

  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(data))
      .Times(1);

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  auth->set_password_hash(hash_ascii_);
  auth->set_username(username_);
  auth->OnURLFetchComplete(NULL, source, status, 403, cookies_, data);
  message_loop.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, OfflineLoginTest) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  int error_no = ECONNRESET;
  std::string data(strerror(error_no));
  GURL source;

  URLRequestStatus status(URLRequestStatus::FAILED, error_no);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, data_))
      .Times(1);
  EXPECT_CALL(*mock_library_, CheckKey(username_, hash_ascii_))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_library_, Mount(username_, hash_ascii_))
      .WillOnce(Return(true));

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  auth->set_password_hash(hash_ascii_);
  auth->set_username(username_);
  auth->OnURLFetchComplete(NULL, source, status, 0, cookies_, data);
  message_loop.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, OnlineLoginTest) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  GURL source(AuthResponseHandler::kTokenAuthUrl);
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, data_))
      .Times(1);
  EXPECT_CALL(*mock_library_, Mount(username_, hash_ascii_))
      .WillOnce(Return(true));

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  auth->set_password_hash(hash_ascii_);
  auth->set_username(username_);
  auth->OnURLFetchComplete(NULL,
                          source,
                          status,
                          kHttpSuccess,
                          cookies_,
                          std::string());
  message_loop.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, LocalaccountLoginTest) {
  GURL source(AuthResponseHandler::kTokenAuthUrl);
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);

  std::string trigger(GoogleAuthenticator::kTmpfsTrigger);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, _))
      .Times(1);
  EXPECT_CALL(*mock_library_, Mount(trigger, _))
      .WillOnce(Return(true));

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  auth->set_password_hash(hash_ascii_);
  auth->set_username(username_);
  auth->set_localaccount(username_);

  auth->CheckLocalaccount(std::string());
}

// Responds as though ClientLogin was successful.
class MockFetcher : public URLFetcher {
 public:
  MockFetcher(const GURL& url,
              URLFetcher::RequestType request_type,
              URLFetcher::Delegate* d)
      : URLFetcher(url, request_type, d) {
  }
  ~MockFetcher() {}
  void Start() {
    GURL source(AuthResponseHandler::kClientLoginUrl);
    URLRequestStatus status(URLRequestStatus::SUCCESS, 0);
    delegate()->OnURLFetchComplete(NULL,
                                   source,
                                   status,
                                   kHttpSuccess,
                                   ResponseCookies(),
                                   std::string());
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(MockFetcher);
};

class MockFactory : public URLFetcher::Factory {
 public:
  MockFactory() {}
  ~MockFactory() {}
  URLFetcher* CreateURLFetcher(int id,
                               const GURL& url,
                               URLFetcher::RequestType request_type,
                               URLFetcher::Delegate* d) {
    return new MockFetcher(url, request_type, d);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(MockFactory);
};

TEST_F(GoogleAuthenticatorTest, FullLoginTest) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);
  ChromeThread file_thread(ChromeThread::FILE);
  file_thread.Start();

  GURL source(AuthResponseHandler::kTokenAuthUrl);
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, data_))
      .Times(1);
  EXPECT_CALL(*mock_library_, Mount(username_, _))
      .WillOnce(Return(true));

  TestingProfile profile;

  MockFactory factory;
  URLFetcher::set_factory(&factory);
  std::vector<unsigned char> salt_v(fake_hash_,
                                    fake_hash_ + sizeof(fake_hash_));

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  auth->set_system_salt(salt_v);

  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(auth.get(),
                        &Authenticator::Authenticate,
                        &profile, username_, hash_ascii_));
  file_thread.Stop();
  message_loop.RunAllPending();
  URLFetcher::set_factory(NULL);
}

}  // namespace chromeos
