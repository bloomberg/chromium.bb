// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/login/client_login_response_handler.h"
#include "chrome/browser/chromeos/login/google_authenticator.h"
#include "chrome/browser/chromeos/login/issue_response_handler.h"
#include "chrome/browser/chromeos/login/mock_auth_response_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/gaia/gaia_authenticator2_unittest.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace file_util;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::_;

namespace chromeos {

class MockConsumer : public LoginStatusConsumer {
 public:
  MockConsumer() {}
  ~MockConsumer() {}
  MOCK_METHOD1(OnLoginFailure, void(const std::string& error));
  MOCK_METHOD2(OnLoginSuccess, void(const std::string& username,
      const GaiaAuthConsumer::ClientLoginResult& result));
  MOCK_METHOD0(OnOffTheRecordLoginSuccess, void(void));
  MOCK_METHOD1(OnPasswordChangeDetected,
      void(const GaiaAuthConsumer::ClientLoginResult& result));
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
    ChromeThread file_thread(ChromeThread::FILE);
    file_thread.Start();

    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(auth,
                          &GoogleAuthenticator::LoadLocalaccount,
                          filename));
  }

  void PrepForLogin(GoogleAuthenticator* auth) {
    auth->set_password_hash(hash_ascii_);
    auth->set_username(username_);
    auth->SetLocalaccount("");
  }

  unsigned char fake_hash_[32];
  std::string hash_ascii_;
  std::string username_;
  GaiaAuthConsumer::ClientLoginResult result_;
  // Mocks, destroyed by CrosLibrary class.
  MockCryptohomeLibrary* mock_library_;
  MockLibraryLoader* loader_;
  char raw_bytes_[2];
  std::string bytes_as_ascii_;
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

TEST_F(GoogleAuthenticatorTest, EmailAddressNoOp) {
  const char lower_case[] = "user@what.com";
  EXPECT_EQ(lower_case, GoogleAuthenticator::Canonicalize(lower_case));
}

TEST_F(GoogleAuthenticatorTest, EmailAddressIgnoreCaps) {
  EXPECT_EQ(GoogleAuthenticator::Canonicalize("user@what.com"),
            GoogleAuthenticator::Canonicalize("UsEr@what.com"));
}

TEST_F(GoogleAuthenticatorTest, EmailAddressIgnoreDomainCaps) {
  EXPECT_EQ(GoogleAuthenticator::Canonicalize("user@what.com"),
            GoogleAuthenticator::Canonicalize("UsEr@what.COM"));
}

TEST_F(GoogleAuthenticatorTest, EmailAddressIgnoreOneUsernameDot) {
  EXPECT_EQ(GoogleAuthenticator::Canonicalize("us.er@what.com"),
            GoogleAuthenticator::Canonicalize("UsEr@what.com"));
}

TEST_F(GoogleAuthenticatorTest, EmailAddressIgnoreManyUsernameDots) {
  EXPECT_EQ(GoogleAuthenticator::Canonicalize("u.ser@what.com"),
            GoogleAuthenticator::Canonicalize("Us.E.r@what.com"));
}

TEST_F(GoogleAuthenticatorTest, EmailAddressIgnoreConsecutiveUsernameDots) {
  EXPECT_EQ(GoogleAuthenticator::Canonicalize("use.r@what.com"),
            GoogleAuthenticator::Canonicalize("Us....E.r@what.com"));
}

TEST_F(GoogleAuthenticatorTest, EmailAddressDifferentOnesRejected) {
  EXPECT_NE(GoogleAuthenticator::Canonicalize("who@what.com"),
            GoogleAuthenticator::Canonicalize("Us....E.r@what.com"));
}

TEST_F(GoogleAuthenticatorTest, EmailAddressIgnorePlusSuffix) {
  EXPECT_EQ(GoogleAuthenticator::Canonicalize("user+cc@what.com"),
            GoogleAuthenticator::Canonicalize("user@what.com"));
}

TEST_F(GoogleAuthenticatorTest, EmailAddressIgnoreMultiPlusSuffix) {
  EXPECT_EQ(GoogleAuthenticator::Canonicalize("user+cc+bcc@what.com"),
            GoogleAuthenticator::Canonicalize("user@what.com"));
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
      FakeLocalaccountFile(StringPrintf("%s\n", bytes_as_ascii_.c_str()));

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
  EXPECT_CALL(consumer, OnLoginSuccess(username_, _))
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*mock_library_, Mount(username_, hash_ascii_, _))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  auth->set_password_hash(hash_ascii_);
  auth->set_username(username_);
  auth->OnLoginSuccess(result_);
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
  auth->OnLoginSuccess(result_);
}

TEST_F(GoogleAuthenticatorTest, PasswordChange) {
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnPasswordChangeDetected(result_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(consumer, OnLoginSuccess(username_, result_))
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
  auth->OnLoginSuccess(result_);
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
  auth->OnLoginSuccess(result_);
  auth->RecoverEncryptedData("whaty", result_);
}

TEST_F(GoogleAuthenticatorTest, ForgetOldData) {
  MockConsumer consumer;
  EXPECT_CALL(consumer, OnPasswordChangeDetected(result_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(consumer, OnLoginSuccess(username_, result_))
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
  auth->OnLoginSuccess(result_);
  auth->ResyncEncryptedData(result_);
}

TEST_F(GoogleAuthenticatorTest, LoginNetFailure) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  int error_no = net::ERR_CONNECTION_RESET;
  std::string data(net::ErrorToString(error_no));

  GaiaAuthConsumer::GaiaAuthError error;
  error.code = GaiaAuthConsumer::NETWORK_ERROR;
  error.network_error = error_no;

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(data))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, CheckKey(username_, hash_ascii_))
      .WillOnce(Return(false))
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForLogin(auth.get());
  auth->OnClientLoginFailure(error);
  message_loop.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, LoginDenied) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  GaiaAuthConsumer::GaiaAuthError error;
  error.code = GaiaAuthConsumer::PERMISSION_DENIED;

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(_))
      .Times(1)
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForLogin(auth.get());
  auth->OnClientLoginFailure(error);
  message_loop.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, CaptchaErrorOutputted) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  // TODO(chron): Swap out this captcha passing for actual captcha parsing.
  GaiaAuthConsumer::GaiaAuthError error;
  error.code = GaiaAuthConsumer::PERMISSION_DENIED;
  error.data = "Url=http://www.google.com/login/captcha\n"
               "Error=CaptchaRequired\n"
               "CaptchaToken=DQAAAGgA...dkI1LK9\n"
               "CaptchaUrl=Captcha?ctoken=...\n";

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginFailure(error.data))
      .Times(1)
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForLogin(auth.get());
  auth->OnClientLoginFailure(error);
  message_loop.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, OfflineLogin) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  GaiaAuthConsumer::GaiaAuthError error;
  error.code = GaiaAuthConsumer::NETWORK_ERROR;
  error.network_error = net::ERR_CONNECTION_RESET;

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, result_))
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
  auth->OnClientLoginFailure(error);
  message_loop.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, OnlineLogin) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, result_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, Mount(username_, hash_ascii_, _))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForLogin(auth.get());
  auth->OnClientLoginSuccess(result_);
  message_loop.RunAllPending();
}

TEST_F(GoogleAuthenticatorTest, CheckLocalaccount) {
  GURL source(AuthResponseHandler::kTokenAuthUrl);
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, MountForBwsi(_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  PrepForLogin(auth.get());
  auth->SetLocalaccount(username_);

  auth->CheckLocalaccount(std::string());
}

// Compatible with LoginStatusConsumer::OnLoginSuccess()
static void Quit(const std::string& username,
                 const GaiaAuthConsumer::ClientLoginResult& credentials) {
  MessageLoop::current()->Quit();
}
// Compatible with LoginStatusConsumer::OnLoginFailure()
static void QuitAndFail(const std::string& error) {
  ADD_FAILURE() << "Login should have succeeded!";
  MessageLoop::current()->Quit();
}

TEST_F(GoogleAuthenticatorTest, LocalaccountLogin) {
  // This test checks the logic that governs asynchronously reading the
  // localaccount name off disk and trying to authenticate against it
  // simultaneously.
  MessageLoop message_loop(MessageLoop::TYPE_UI);
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);

  MockConsumer consumer;
  ON_CALL(consumer, OnLoginSuccess(username_, _))
      .WillByDefault(Invoke(Quit));
  EXPECT_CALL(consumer, OnLoginSuccess(username_, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, MountForBwsi(_))
      .WillOnce(Return(true))
      .RetiresOnSaturation();
  // Enable the test to terminate (and fail), even if the login fails.
  ON_CALL(consumer, OnLoginFailure(_))
      .WillByDefault(Invoke(QuitAndFail));

  // Manually prep for login, so that localaccount isn't set for us.
  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  auth->set_password_hash(hash_ascii_);
  auth->set_username(username_);

  // First, force a check of username_ against the localaccount -- which we
  // haven't yet gotten off disk.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(auth.get(),
                        &GoogleAuthenticator::CheckLocalaccount,
                        std::string("fail")));
  message_loop.RunAllPending();
  // The foregoing has now rescheduled itself in a few ms because we don't
  // yet have the localaccount loaded off disk.

  // Now, cause the FILE thread to go load the localaccount off disk.
  FilePath tmp_file_path = FakeLocalaccountFile(username_);
  ReadLocalaccountFile(auth.get(), tmp_file_path.BaseName().value());

  // Run remaining events, until OnLoginSuccess or OnLoginFailure is called.
  message_loop.Run();

  // Cleanup.
  Delete(tmp_file_path, false);
}

TEST_F(GoogleAuthenticatorTest, FullLogin) {
  MessageLoopForUI message_loop;
  ChromeThread ui_thread(ChromeThread::UI, &message_loop);
  GURL source(AuthResponseHandler::kTokenAuthUrl);
  URLRequestStatus status(URLRequestStatus::SUCCESS, 0);
  chromeos::CryptohomeBlob salt_v(fake_hash_, fake_hash_ + sizeof(fake_hash_));

  MockConsumer consumer;
  EXPECT_CALL(consumer, OnLoginSuccess(username_, result_))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, Mount(username_, _, _))
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  ON_CALL(*mock_library_, GetSystemSalt())
      .WillByDefault(Return(salt_v));
  EXPECT_CALL(*mock_library_, GetSystemSalt())
      .Times(1)
      .RetiresOnSaturation();

  TestingProfile profile;

  MockFactory factory;
  URLFetcher::set_factory(&factory);

  scoped_refptr<GoogleAuthenticator> auth(new GoogleAuthenticator(&consumer));
  auth->AuthenticateToLogin(
      &profile, username_, hash_ascii_, std::string(), std::string());

  URLFetcher::set_factory(NULL);
  message_loop.RunAllPending();
}

}  // namespace chromeos
