// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/google_authenticator.h"

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/login/client_login_response_handler.h"
#include "chrome/browser/chromeos/login/issue_response_handler.h"
#include "chrome/browser/chromeos/login/mock_auth_response_handler.h"
#include "chrome/browser/chromeos/login/mock_login_status_consumer.h"
#include "chrome/browser/chromeos/login/mock_url_fetchers.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher_unittest.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace file_util;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::_;

namespace chromeos {

class GoogleAuthenticatorTest : public ::testing::Test {
 public:
  GoogleAuthenticatorTest()
      : message_loop_ui_(MessageLoop::TYPE_UI),
        ui_thread_(BrowserThread::UI, &message_loop_ui_),
        username_("me@nowhere.org"),
        password_("fakepass"),
        result_("", "", "", ""),
        bytes_as_ascii_("ffff"),
        user_manager_(new MockUserManager) {
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

    test_api->SetLibraryLoader(loader_, true);

    mock_library_ = new MockCryptohomeLibrary();
    test_api->SetCryptohomeLibrary(mock_library_, true);
  }

  // Tears down the test fixture.
  virtual void TearDown() {
    // Prevent bogus gMock leak check from firing.
    chromeos::CrosLibrary::TestApi* test_api =
        chromeos::CrosLibrary::Get()->GetTestApi();
    test_api->SetLibraryLoader(NULL, false);
    test_api->SetCryptohomeLibrary(NULL, false);
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

  void ReadLocalaccountFile(GoogleAuthenticator* auth,
                            const std::string& filename) {
    BrowserThread file_thread(BrowserThread::FILE);
    file_thread.Start();

    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(auth,
                          &GoogleAuthenticator::LoadLocalaccount,
                          filename));
  }

  void PrepForLogin(GoogleAuthenticator* auth) {
    auth->set_password_hash(hash_ascii_);
    auth->set_username(username_);
    auth->set_password(password_);
    auth->SetLocalaccount("");
    auth->set_user_manager(user_manager_.get());
    ON_CALL(*user_manager_.get(), IsKnownUser(username_))
        .WillByDefault(Return(true));
  }

  void PrepForFailedLogin(GoogleAuthenticator* auth) {
    PrepForLogin(auth);
    auth->set_hosted_policy(GaiaAuthFetcher::HostedAccountsAllowed);
  }

  void CancelLogin(GoogleAuthenticator* auth) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        NewRunnableMethod(auth,
                          &GoogleAuthenticator::CancelClientLogin));
  }

  MessageLoop message_loop_ui_;
  BrowserThread ui_thread_;

  unsigned char fake_hash_[32];
  std::string hash_ascii_;
  std::string username_;
  std::string password_;
  GaiaAuthConsumer::ClientLoginResult result_;
  // Mocks, destroyed by CrosLibrary class.
  MockCryptohomeLibrary* mock_library_;
  MockLibraryLoader* loader_;

  char raw_bytes_[2];
  std::string bytes_as_ascii_;

  scoped_ptr<MockUserManager> user_manager_;
};

TEST_F(GoogleAuthenticatorTest, SaltToAscii) {
  unsigned char fake_salt[8] = { 0 };
  fake_salt[0] = 10;
  fake_salt[1] = 1;
  fake_salt[7] = 10 << 4;
  std::vector<unsigned char> salt_v(fake_salt, fake_salt + sizeof(fake_salt));

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(NULL));

  ON_CALL(*mock_library_, GetSystemSalt())
      .WillByDefault(Return(salt_v));
  EXPECT_CALL(*mock_library_, GetSystemSalt())
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_EQ("0a010000000000a0", auth->SaltAsAscii());
}

TEST_F(GoogleAuthenticatorTest, ReadLocalaccount) {
  FilePath tmp_file_path = FakeLocalaccountFile(bytes_as_ascii_);

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(NULL));
  ReadLocalaccountFile(auth.get(), tmp_file_path.BaseName().value());
  EXPECT_EQ(auth->localaccount_, bytes_as_ascii_);
  Delete(tmp_file_path, false);
}

TEST_F(GoogleAuthenticatorTest, ReadLocalaccountTrailingWS) {
  FilePath tmp_file_path =
      FakeLocalaccountFile(base::StringPrintf("%s\n",
                                              bytes_as_ascii_.c_str()));

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(NULL));
  ReadLocalaccountFile(auth.get(), tmp_file_path.BaseName().value());
  EXPECT_EQ(auth->localaccount_, bytes_as_ascii_);
  Delete(tmp_file_path, false);
}

TEST_F(GoogleAuthenticatorTest, ReadNoLocalaccount) {
  FilePath tmp_file_path = FakeLocalaccountFile(bytes_as_ascii_);
  EXPECT_TRUE(Delete(tmp_file_path, false));  // Ensure non-existent file.

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(NULL));
  ReadLocalaccountFile(auth.get(), tmp_file_path.BaseName().value());
  EXPECT_EQ(auth->localaccount_, std::string());
}

TEST_F(GoogleAuthenticatorTest, OnLoginSuccess) {
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, password_, _, false))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*mock_library_, Mount(username_, hash_ascii_, _))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  auth->set_password_hash(hash_ascii_);
  auth->set_username(username_);
  auth->set_password(password_);
  auth->OnLoginSuccess(result_, false);
}

TEST_F(GoogleAuthenticatorTest, MountFailure) {
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(_))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*mock_library_, Mount(username_, hash_ascii_, _))
      .WillOnce(Return(false))
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForLogin(auth.get());
  auth->OnLoginSuccess(result_, false);
}

TEST_F(GoogleAuthenticatorTest, PasswordChange) {
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnPasswordChangeDetected(result_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(consumer, OnLoginSuccess(username_, password_, result_, false))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*mock_library_, Mount(username_, hash_ascii_, _))
      .WillOnce(
          DoAll(SetArgumentPointee<2>(
              chromeos::kCryptohomeMountErrorKeyFailure),
                Return(false)))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  EXPECT_CALL(*mock_library_, GetSystemSalt())
      .WillOnce(Return(chromeos::CryptohomeBlob(8, 'a')))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, MigrateKey(username_, _, hash_ascii_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForLogin(auth.get());
  auth->OnLoginSuccess(result_, false);
  auth->RecoverEncryptedData("whaty", result_);
}

TEST_F(GoogleAuthenticatorTest, PasswordChangeWrongPassword) {
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnPasswordChangeDetected(result_))
      .Times(2)
      .RetiresOnSaturation();

  EXPECT_CALL(*mock_library_, Mount(username_, hash_ascii_, _))
      .WillOnce(
          DoAll(SetArgumentPointee<2>(
              chromeos::kCryptohomeMountErrorKeyFailure),
                Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, GetSystemSalt())
      .WillOnce(Return(chromeos::CryptohomeBlob(8, 'a')))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, MigrateKey(username_, _, hash_ascii_))
      .WillOnce(Return(false))
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForLogin(auth.get());
  auth->OnLoginSuccess(result_, false);
  auth->RecoverEncryptedData("whaty", result_);
}

TEST_F(GoogleAuthenticatorTest, ForgetOldData) {
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnPasswordChangeDetected(result_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(consumer, OnLoginSuccess(username_, password_, result_, false))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*mock_library_, Mount(username_, hash_ascii_, _))
      .WillOnce(
          DoAll(SetArgumentPointee<2>(
              chromeos::kCryptohomeMountErrorKeyFailure),
                Return(false)))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  EXPECT_CALL(*mock_library_, Remove(username_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForLogin(auth.get());
  auth->OnLoginSuccess(result_, false);
  auth->ResyncEncryptedData(result_);
}

TEST_F(GoogleAuthenticatorTest, LoginNetFailure) {
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromConnectionError(net::ERR_CONNECTION_RESET);

  LoginFailure failure =
      LoginFailure::FromNetworkAuthFailure(error);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(failure))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, CheckKey(username_, hash_ascii_))
      .WillOnce(Return(false))
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForLogin(auth.get());
  auth->OnClientLoginFailure(error);
  message_loop_ui_.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, LoginDenied) {
  GoogleServiceAuthError client_error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(_))
      .Times(1)
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForFailedLogin(auth.get());
  EXPECT_CALL(*user_manager_.get(), IsKnownUser(username_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  auth->OnClientLoginFailure(client_error);
  message_loop_ui_.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, LoginAccountDisabled) {
  GoogleServiceAuthError client_error(
      GoogleServiceAuthError::ACCOUNT_DISABLED);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(_))
      .Times(1)
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForFailedLogin(auth.get());
  auth->OnClientLoginFailure(client_error);
  message_loop_ui_.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, LoginAccountDeleted) {
  GoogleServiceAuthError client_error(
      GoogleServiceAuthError::ACCOUNT_DELETED);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(_))
      .Times(1)
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForFailedLogin(auth.get());
  auth->OnClientLoginFailure(client_error);
  message_loop_ui_.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, LoginServiceUnavailable) {
  GoogleServiceAuthError client_error(
      GoogleServiceAuthError::SERVICE_UNAVAILABLE);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(_))
      .Times(1)
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForFailedLogin(auth.get());
  auth->OnClientLoginFailure(client_error);
  message_loop_ui_.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, CaptchaErrorOutputted) {
  GoogleServiceAuthError auth_error =
      GoogleServiceAuthError::FromCaptchaChallenge(
          "CCTOKEN",
          GURL("http://www.google.com/accounts/Captcha?ctoken=CCTOKEN"),
          GURL("http://www.google.com/login/captcha"));

  LoginFailure failure = LoginFailure::FromNetworkAuthFailure(auth_error);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(failure))
      .Times(1)
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForFailedLogin(auth.get());
  auth->OnClientLoginFailure(auth_error);
  message_loop_ui_.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, OfflineLogin) {
  GoogleServiceAuthError auth_error(
      GoogleServiceAuthError::FromConnectionError(net::ERR_CONNECTION_RESET));

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, password_, result_, false))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, CheckKey(username_, hash_ascii_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, Mount(username_, hash_ascii_, _))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForLogin(auth.get());
  auth->OnClientLoginFailure(auth_error);
  message_loop_ui_.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, OnlineLogin) {
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, password_, result_, false))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, Mount(username_, hash_ascii_, _))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForLogin(auth.get());
  EXPECT_CALL(*user_manager_.get(), IsKnownUser(username_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  auth->OnClientLoginSuccess(result_);
  message_loop_ui_.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, CheckLocalaccount) {
  GURL source(AuthResponseHandler::kTokenAuthUrl);
  net::URLRequestStatus status(net::URLRequestStatus::SUCCESS, 0);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, std::string(), _, false))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, MountForBwsi(_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForLogin(auth.get());
  auth->SetLocalaccount(username_);

  auth->CheckLocalaccount(LoginFailure(LoginFailure::LOGIN_TIMED_OUT));
}

TEST_F(GoogleAuthenticatorTest, LocalaccountLogin) {
  // This test checks the logic that governs asynchronously reading the
  // localaccount name off disk and trying to authenticate against it
  // simultaneously.
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, std::string(), _, false))
      .WillOnce(Invoke(MockConsumer::OnSuccessQuit))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, MountForBwsi(_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  // Enable the test to terminate (and fail), even if the login fails.
  ON_CALL(consumer, OnLoginFailure(_))
      .WillByDefault(Invoke(MockConsumer::OnFailQuitAndFail));

  // Manually prep for login, so that localaccount isn't set for us.
  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  auth->set_password_hash(hash_ascii_);
  auth->set_username(username_);

  // First, force a check of username_ against the localaccount -- which we
  // haven't yet gotten off disk.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(auth.get(),
                        &GoogleAuthenticator::CheckLocalaccount,
                        LoginFailure(LoginFailure::LOGIN_TIMED_OUT)));
  message_loop_ui_.RunAllPending();
  // The foregoing has now rescheduled itself in a few ms because we don't
  // yet have the localaccount loaded off disk.

  // Now, cause the FILE thread to go load the localaccount off disk.
  FilePath tmp_file_path = FakeLocalaccountFile(username_);
  ReadLocalaccountFile(auth.get(), tmp_file_path.BaseName().value());

  // Run remaining events, until OnLoginSuccess or OnLoginFailure is called.
  message_loop_ui_.Run();

  // Cleanup.
  Delete(tmp_file_path, false);
}

TEST_F(GoogleAuthenticatorTest, FullLogin) {
  chromeos::CryptohomeBlob salt_v(fake_hash_, fake_hash_ + sizeof(fake_hash_));

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_,
                                       password_,
                                       Eq(result_),
                                       false))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, Mount(username_, _, _))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  EXPECT_CALL(*mock_library_, GetSystemSalt())
      .WillOnce(Return(salt_v))
      .RetiresOnSaturation();

  TestingProfile profile;

  MockFactory<MockFetcher> factory;
  URLFetcher::set_factory(&factory);

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  EXPECT_CALL(*user_manager_.get(), IsKnownUser(username_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  auth->set_user_manager(user_manager_.get());
  auth->AuthenticateToLogin(
      &profile, username_, password_, std::string(), std::string());

  URLFetcher::set_factory(NULL);
  message_loop_ui_.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, FullHostedLoginFailure) {
  chromeos::CryptohomeBlob salt_v(fake_hash_, fake_hash_ + sizeof(fake_hash_));

  LoginFailure failure_details =
      LoginFailure::FromNetworkAuthFailure(
          GoogleServiceAuthError(
              GoogleServiceAuthError::HOSTED_NOT_ALLOWED));

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(failure_details))
      .WillOnce(Invoke(MockConsumer::OnFailQuit))
      .RetiresOnSaturation();
  // A failure case, but we still want the test to finish gracefully.
  ON_CALL(consumer, OnLoginSuccess(username_, password_, _, _))
      .WillByDefault(Invoke(MockConsumer::OnSuccessQuitAndFail));

  EXPECT_CALL(*mock_library_, GetSystemSalt())
      .WillOnce(Return(salt_v))
      .RetiresOnSaturation();

  TestingProfile profile;

  MockFactory<HostedFetcher> factory_invalid;
  URLFetcher::set_factory(&factory_invalid);

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  auth->set_user_manager(user_manager_.get());
  EXPECT_CALL(*user_manager_.get(), IsKnownUser(username_))
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  auth->AuthenticateToLogin(
      &profile, username_, hash_ascii_, std::string(), std::string());

  // For when |auth| tries to load the localaccount file.
  BrowserThread file_thread(BrowserThread::FILE);
  file_thread.Start();

  // Run the UI thread until we exit it gracefully.
  message_loop_ui_.Run();
  URLFetcher::set_factory(NULL);
}

TEST_F(GoogleAuthenticatorTest, CancelLogin) {
  chromeos::CryptohomeBlob salt_v(fake_hash_, fake_hash_ + sizeof(fake_hash_));

  MockConsumer consumer;
  // The expected case.
  EXPECT_CALL(consumer, OnLoginFailure(_))
      .WillOnce(Invoke(MockConsumer::OnFailQuit))
      .RetiresOnSaturation();

  // A failure case, but we still want the test to finish gracefully.
  ON_CALL(consumer, OnLoginSuccess(username_, password_, _, _))
      .WillByDefault(Invoke(MockConsumer::OnSuccessQuitAndFail));

  // Stuff we expect to happen along the way.
  EXPECT_CALL(*mock_library_, GetSystemSalt())
      .WillOnce(Return(salt_v))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, CheckKey(username_, _))
      .WillOnce(Return(false))
      .RetiresOnSaturation();

  TestingProfile profile;

  // This is how we inject fake URLFetcher objects, with a factory.
  // This factory creates fake URLFetchers that Start() a fake fetch attempt
  // and then come back on the UI thread after a small delay.  They expect to
  // be canceled before they come back, and the test will fail if they are not.
  MockFactory<ExpectCanceledFetcher> factory;
  URLFetcher::set_factory(&factory);

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  // For when |auth| tries to load the localaccount file.
  BrowserThread file_thread(BrowserThread::FILE);
  file_thread.Start();

  // Start an authentication attempt, which will kick off a URL "fetch" that
  // we expect to cancel before it completes.
  auth->AuthenticateToLogin(
      &profile, username_, hash_ascii_, std::string(), std::string());

  // Post a task to cancel the login attempt.
  CancelLogin(auth.get());

  URLFetcher::set_factory(NULL);

  // Run the UI thread until we exit it gracefully.
  message_loop_ui_.Run();
}

TEST_F(GoogleAuthenticatorTest, CancelLoginAlreadyGotLocalaccount) {
  chromeos::CryptohomeBlob salt_v(fake_hash_, fake_hash_ + sizeof(fake_hash_));

  MockConsumer consumer;
  // The expected case.
  EXPECT_CALL(consumer, OnLoginFailure(_))
      .WillOnce(Invoke(MockConsumer::OnFailQuit))
      .RetiresOnSaturation();

  // A failure case, but we still want the test to finish gracefully.
  ON_CALL(consumer, OnLoginSuccess(username_, password_, _, _))
      .WillByDefault(Invoke(MockConsumer::OnSuccessQuitAndFail));

  // Stuff we expect to happen along the way.
  EXPECT_CALL(*mock_library_, GetSystemSalt())
      .WillOnce(Return(salt_v))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, CheckKey(username_, _))
      .WillOnce(Return(false))
      .RetiresOnSaturation();

  TestingProfile profile;

  // This is how we inject fake URLFetcher objects, with a factory.
  // This factory creates fake URLFetchers that Start() a fake fetch attempt
  // and then come back on the UI thread after a small delay.  They expect to
  // be canceled before they come back, and the test will fail if they are not.
  MockFactory<ExpectCanceledFetcher> factory;
  URLFetcher::set_factory(&factory);

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  // This time, instead of allowing |auth| to go get the localaccount file
  // itself, we simulate the case where the file is already loaded, which
  // happens when this isn't the first login since chrome started.
  ReadLocalaccountFile(auth.get(), "");

  // Start an authentication attempt, which will kick off a URL "fetch" that
  // we expect to cancel before it completes.
  auth->AuthenticateToLogin(
      &profile, username_, hash_ascii_, std::string(), std::string());

  // Post a task to cancel the login attempt.
  CancelLogin(auth.get());

  URLFetcher::set_factory(NULL);

  // Run the UI thread until we exit it gracefully.
  message_loop_ui_.Run();
}

}  // namespace chromeos
