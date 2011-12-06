// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/parallel_authenticator.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/test/thread_test_helper.h"
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/login/mock_auth_response_handler.h"
#include "chrome/browser/chromeos/login/mock_login_status_consumer.h"
#include "chrome/browser/chromeos/login/mock_url_fetchers.h"
#include "chrome/browser/chromeos/login/test_attempt_state.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher_unittest.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using file_util::CloseFile;
using file_util::CreateAndOpenTemporaryFile;
using file_util::CreateAndOpenTemporaryFileInDir;
using file_util::Delete;
using file_util::WriteFile;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::_;

namespace chromeos {

class ResolveChecker : public base::ThreadTestHelper {
 public:
  ResolveChecker(TestAttemptState* state,
                 ParallelAuthenticator* auth,
                 ParallelAuthenticator::AuthState expected)
      : base::ThreadTestHelper(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)),
        state_(state),
        auth_(auth),
        expected_(expected) {
  }
  ~ResolveChecker() {}

  virtual void RunTest() {
    auth_->set_attempt_state(state_);
    set_test_result(expected_ == auth_->ResolveState());
  }

 private:
  TestAttemptState* state_;
  ParallelAuthenticator* auth_;
  ParallelAuthenticator::AuthState expected_;
};

class TestOnlineAttempt : public OnlineAttempt {
 public:
  TestOnlineAttempt(AuthAttemptState* state,
                    AuthAttemptStateResolver* resolver)
      : OnlineAttempt(false, state, resolver) {
  }
};

class ParallelAuthenticatorTest : public testing::Test {
 public:
  ParallelAuthenticatorTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE),
        io_thread_(BrowserThread::IO),
        username_("me@nowhere.org"),
        password_("fakepass") {
    hash_ascii_.assign("0a010000000000a0");
    hash_ascii_.append(std::string(16, '0'));
  }

  ~ParallelAuthenticatorTest() {}

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
    file_thread_.Start();
    io_thread_.Start();

    auth_ = new ParallelAuthenticator(&consumer_);
    state_.reset(new TestAttemptState(username_,
                                      password_,
                                      hash_ascii_,
                                      "",
                                      "",
                                      false));
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
    EXPECT_NE(tmp_file, static_cast<FILE*>(NULL));
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
    EXPECT_NE(tmp_file, static_cast<FILE*>(NULL));
    EXPECT_EQ(WriteFile(local_account_file, ascii.c_str(), ascii_len),
              ascii_len);
    EXPECT_TRUE(CloseFile(tmp_file));
    return local_account_file;
  }

  void ReadLocalaccountFile(ParallelAuthenticator* auth,
                            const std::string& filename) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&ParallelAuthenticator::LoadLocalaccount, auth, filename));
    file_thread_.Stop();
    file_thread_.Start();
  }

  // Allow test to fail and exit gracefully, even if OnLoginFailure()
  // wasn't supposed to happen.
  void FailOnLoginFailure() {
    ON_CALL(consumer_, OnLoginFailure(_))
        .WillByDefault(Invoke(MockConsumer::OnFailQuitAndFail));
  }

  // Allow test to fail and exit gracefully, even if OnLoginSuccess()
  // wasn't supposed to happen.
  void FailOnLoginSuccess() {
    ON_CALL(consumer_, OnLoginSuccess(_, _, _, _, _))
        .WillByDefault(Invoke(MockConsumer::OnSuccessQuitAndFail));
  }

  // Allow test to fail and exit gracefully, even if
  // OnOffTheRecordLoginSuccess() wasn't supposed to happen.
  void FailOnGuestLoginSuccess() {
    ON_CALL(consumer_, OnOffTheRecordLoginSuccess())
        .WillByDefault(Invoke(MockConsumer::OnGuestSuccessQuitAndFail));
  }

  void ExpectLoginFailure(const LoginFailure& failure) {
    EXPECT_CALL(consumer_, OnLoginFailure(failure))
        .WillOnce(Invoke(MockConsumer::OnFailQuit))
        .RetiresOnSaturation();
  }

  void ExpectLoginSuccess(const std::string& username,
                          const std::string& password,
                          const GaiaAuthConsumer::ClientLoginResult& result,
                          bool pending) {
    EXPECT_CALL(consumer_, OnLoginSuccess(username, password, result, pending,
                                          false))
        .WillOnce(Invoke(MockConsumer::OnSuccessQuit))
        .RetiresOnSaturation();
  }

  void ExpectGuestLoginSuccess() {
    EXPECT_CALL(consumer_, OnOffTheRecordLoginSuccess())
        .WillOnce(Invoke(MockConsumer::OnGuestSuccessQuit))
        .RetiresOnSaturation();
  }

  void ExpectPasswordChange() {
    EXPECT_CALL(consumer_, OnPasswordChangeDetected(result_))
        .WillOnce(Invoke(MockConsumer::OnMigrateQuit))
        .RetiresOnSaturation();
  }

  void RunResolve(ParallelAuthenticator* auth, MessageLoop* loop) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ParallelAuthenticator::Resolve, auth));
    loop->Run();
  }

  void SetAttemptState(ParallelAuthenticator* auth, TestAttemptState* state) {
    auth->set_attempt_state(state);
  }

  void FakeOnlineAttempt() {
    auth_->set_online_attempt(new TestOnlineAttempt(state_.get(), auth_.get()));
  }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;

  std::string username_;
  std::string password_;
  std::string hash_ascii_;
  GaiaAuthConsumer::ClientLoginResult result_;

  // Initializes / shuts down a stub CrosLibrary.
  chromeos::ScopedStubCrosEnabler stub_cros_enabler_;

  // Mocks, destroyed by CrosLibrary class.
  MockCryptohomeLibrary* mock_library_;
  MockLibraryLoader* loader_;

  MockConsumer consumer_;
  scoped_refptr<ParallelAuthenticator> auth_;
  scoped_ptr<TestAttemptState> state_;
};

TEST_F(ParallelAuthenticatorTest, ReadLocalaccount) {
  FilePath tmp_file_path = FakeLocalaccountFile(username_);

  ReadLocalaccountFile(auth_.get(), tmp_file_path.BaseName().value());
  EXPECT_EQ(auth_->localaccount_, username_);
  Delete(tmp_file_path, false);
}

TEST_F(ParallelAuthenticatorTest, ReadLocalaccountTrailingWS) {
  FilePath tmp_file_path =
      FakeLocalaccountFile(base::StringPrintf("%s\n", username_.c_str()));

  ReadLocalaccountFile(auth_.get(), tmp_file_path.BaseName().value());
  EXPECT_EQ(auth_->localaccount_, username_);
  Delete(tmp_file_path, false);
}

TEST_F(ParallelAuthenticatorTest, ReadNoLocalaccount) {
  FilePath tmp_file_path = FakeLocalaccountFile(username_);
  EXPECT_TRUE(Delete(tmp_file_path, false));  // Ensure non-existent file.

  ReadLocalaccountFile(auth_.get(), tmp_file_path.BaseName().value());
  EXPECT_EQ(auth_->localaccount_, std::string());
}

TEST_F(ParallelAuthenticatorTest, OnLoginSuccess) {
  EXPECT_CALL(consumer_, OnLoginSuccess(username_, password_, result_, false,
                                        false))
      .Times(1)
      .RetiresOnSaturation();

  SetAttemptState(auth_, state_.release());
  auth_->OnLoginSuccess(result_, false);
}

TEST_F(ParallelAuthenticatorTest, OnPasswordChangeDetected) {
  EXPECT_CALL(consumer_, OnPasswordChangeDetected(result_))
      .Times(1)
      .RetiresOnSaturation();
  SetAttemptState(auth_, state_.release());
  auth_->OnPasswordChangeDetected(result_);
}

TEST_F(ParallelAuthenticatorTest, ResolveNothingDone) {
  scoped_refptr<ResolveChecker> checker(
      new ResolveChecker(state_.release(),
                         auth_.get(),
                         ParallelAuthenticator::CONTINUE));
  EXPECT_TRUE(checker->Run());
}

TEST_F(ParallelAuthenticatorTest, ResolvePossiblePwChange) {
  // Set a fake online attempt so that we return intermediate cryptohome state.
  FakeOnlineAttempt();

  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected.
  state_->PresetCryptohomeStatus(false,
                                 chromeos::kCryptohomeMountErrorKeyFailure);
  scoped_refptr<ResolveChecker> checker(
      new ResolveChecker(state_.release(),
                         auth_.get(),
                         ParallelAuthenticator::POSSIBLE_PW_CHANGE));
  EXPECT_TRUE(checker->Run());
}

TEST_F(ParallelAuthenticatorTest, ResolvePossiblePwChangeToFailedMount) {
  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected.
  state_->PresetCryptohomeStatus(false,
                                 chromeos::kCryptohomeMountErrorKeyFailure);

  // When there is no online attempt and online results, POSSIBLE_PW_CHANGE
  // will be resolved to FAILED_MOUNT.
  scoped_refptr<ResolveChecker> checker(
      new ResolveChecker(state_.release(),
                         auth_.get(),
                         ParallelAuthenticator::FAILED_MOUNT));
  EXPECT_TRUE(checker->Run());
}

TEST_F(ParallelAuthenticatorTest, ResolveNeedOldPw) {
  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected because of unmatched key; additionally,
  // an online auth attempt has completed successfully.
  state_->PresetCryptohomeStatus(
      false,
      chromeos::kCryptohomeMountErrorKeyFailure);
  state_->PresetOnlineLoginStatus(GaiaAuthConsumer::ClientLoginResult(),
                                 LoginFailure::None());
  scoped_refptr<ResolveChecker> checker(
      new ResolveChecker(state_.release(),
                         auth_.get(),
                         ParallelAuthenticator::NEED_OLD_PW));
  EXPECT_TRUE(checker->Run());
}

TEST_F(ParallelAuthenticatorTest, DriveFailedMount) {
  FailOnLoginSuccess();
  ExpectLoginFailure(LoginFailure(LoginFailure::COULD_NOT_MOUNT_CRYPTOHOME));

  // Set up state as though a cryptohome mount attempt has occurred
  // and failed.
  state_->PresetCryptohomeStatus(false, 0);
  SetAttemptState(auth_, state_.release());

  RunResolve(auth_.get(), &message_loop_);
}

TEST_F(ParallelAuthenticatorTest, DriveGuestLogin) {
  ExpectGuestLoginSuccess();
  FailOnLoginFailure();

  // Set up mock cryptohome library to respond as though a tmpfs mount
  // attempt has occurred and succeeded.
  mock_library_->SetUp(true, 0);
  EXPECT_CALL(*mock_library_, AsyncMountForBwsi(_))
      .Times(1)
      .RetiresOnSaturation();

  auth_->LoginOffTheRecord();
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveGuestLoginButFail) {
  FailOnGuestLoginSuccess();
  ExpectLoginFailure(LoginFailure(LoginFailure::COULD_NOT_MOUNT_TMPFS));

  // Set up mock cryptohome library to respond as though a tmpfs mount
  // attempt has occurred and failed.
  mock_library_->SetUp(false, 0);
  EXPECT_CALL(*mock_library_, AsyncMountForBwsi(_))
      .Times(1)
      .RetiresOnSaturation();

  auth_->LoginOffTheRecord();
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveDataResync) {
  ExpectLoginSuccess(username_, password_, result_, false);
  FailOnLoginFailure();

  // Set up mock cryptohome library to respond successfully to a cryptohome
  // remove attempt and a cryptohome create attempt (specified by the |true|
  // argument to AsyncMount).
  mock_library_->SetUp(true, 0);
  EXPECT_CALL(*mock_library_, AsyncRemove(username_, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, AsyncMount(username_, hash_ascii_, true, _))
      .Times(1)
      .RetiresOnSaturation();

  state_->PresetOnlineLoginStatus(result_, LoginFailure::None());
  SetAttemptState(auth_, state_.release());

  auth_->ResyncEncryptedData(result_);
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveResyncFail) {
  FailOnLoginSuccess();
  ExpectLoginFailure(LoginFailure(LoginFailure::DATA_REMOVAL_FAILED));

  // Set up mock cryptohome library to fail a cryptohome remove attempt.
  mock_library_->SetUp(false, 0);
  EXPECT_CALL(*mock_library_, AsyncRemove(username_, _))
      .Times(1)
      .RetiresOnSaturation();

  SetAttemptState(auth_, state_.release());

  auth_->ResyncEncryptedData(result_);
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveRequestOldPassword) {
  FailOnLoginSuccess();
  ExpectPasswordChange();

  state_->PresetCryptohomeStatus(false,
                                 chromeos::kCryptohomeMountErrorKeyFailure);
  state_->PresetOnlineLoginStatus(result_, LoginFailure::None());
  SetAttemptState(auth_, state_.release());

  RunResolve(auth_.get(), &message_loop_);
}

TEST_F(ParallelAuthenticatorTest, DriveDataRecover) {
  ExpectLoginSuccess(username_, password_, result_, false);
  FailOnLoginFailure();

  // Set up mock cryptohome library to respond successfully to a key migration.
  mock_library_->SetUp(true, 0);
  EXPECT_CALL(*mock_library_, AsyncMigrateKey(username_, _, hash_ascii_, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, AsyncMount(username_, hash_ascii_, false, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, HashPassword(_))
      .WillOnce(Return(std::string()))
      .RetiresOnSaturation();

  state_->PresetOnlineLoginStatus(result_, LoginFailure::None());
  SetAttemptState(auth_, state_.release());

  auth_->RecoverEncryptedData(std::string(), result_);
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveDataRecoverButFail) {
  FailOnLoginSuccess();
  ExpectPasswordChange();

  // Set up mock cryptohome library to fail a key migration attempt,
  // asserting that the wrong password was used.
  mock_library_->SetUp(false, chromeos::kCryptohomeMountErrorKeyFailure);
  EXPECT_CALL(*mock_library_, AsyncMigrateKey(username_, _, hash_ascii_, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, HashPassword(_))
      .WillOnce(Return(std::string()))
      .RetiresOnSaturation();

  SetAttemptState(auth_, state_.release());

  auth_->RecoverEncryptedData(std::string(), result_);
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, ResolveNoMount) {
  // Set a fake online attempt so that we return intermediate cryptohome state.
  FakeOnlineAttempt();

  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected because the user doesn't exist.
  state_->PresetCryptohomeStatus(
      false,
      chromeos::kCryptohomeMountErrorUserDoesNotExist);
  scoped_refptr<ResolveChecker> checker(
      new ResolveChecker(state_.release(),
                         auth_.get(),
                         ParallelAuthenticator::NO_MOUNT));
  EXPECT_TRUE(checker->Run());
}

TEST_F(ParallelAuthenticatorTest, ResolveNoMountToFailedMount) {
  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected because the user doesn't exist.
  state_->PresetCryptohomeStatus(
      false,
      chromeos::kCryptohomeMountErrorUserDoesNotExist);

  // When there is no online attempt and online results, NO_MOUNT will be
  // resolved to FAILED_MOUNT.
  scoped_refptr<ResolveChecker> checker(
      new ResolveChecker(state_.release(),
                         auth_.get(),
                         ParallelAuthenticator::FAILED_MOUNT));
  EXPECT_TRUE(checker->Run());
}

TEST_F(ParallelAuthenticatorTest, ResolveCreateNew) {
  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected because the user doesn't exist; additionally,
  // an online auth attempt has completed successfully.
  state_->PresetCryptohomeStatus(
      false,
      chromeos::kCryptohomeMountErrorUserDoesNotExist);
  state_->PresetOnlineLoginStatus(GaiaAuthConsumer::ClientLoginResult(),
                                 LoginFailure::None());
  scoped_refptr<ResolveChecker> checker(
      new ResolveChecker(state_.release(),
                         auth_.get(),
                         ParallelAuthenticator::CREATE_NEW));
  EXPECT_TRUE(checker->Run());
}

TEST_F(ParallelAuthenticatorTest, DriveCreateForNewUser) {
  ExpectLoginSuccess(username_, password_, result_, false);
  FailOnLoginFailure();

  // Set up mock cryptohome library to respond successfully to a cryptohome
  // create attempt (specified by the |true| argument to AsyncMount).
  mock_library_->SetUp(true, 0);
  EXPECT_CALL(*mock_library_, AsyncMount(username_, hash_ascii_, true, _))
      .Times(1)
      .RetiresOnSaturation();

  // Set up state as though a cryptohome mount attempt has occurred
  // and been rejected because the user doesn't exist; additionally,
  // an online auth attempt has completed successfully.
  state_->PresetCryptohomeStatus(
      false,
      chromeos::kCryptohomeMountErrorUserDoesNotExist);
  state_->PresetOnlineLoginStatus(GaiaAuthConsumer::ClientLoginResult(),
                                 LoginFailure::None());
  SetAttemptState(auth_, state_.release());

  RunResolve(auth_.get(), &message_loop_);
}

TEST_F(ParallelAuthenticatorTest, DriveOfflineLogin) {
  ExpectLoginSuccess(username_, password_, result_, false);
  FailOnLoginFailure();

  // Set up state as though a cryptohome mount attempt has occurred and
  // succeeded.
  state_->PresetCryptohomeStatus(true, 0);
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromConnectionError(net::ERR_CONNECTION_RESET);
  state_->PresetOnlineLoginStatus(result_,
                                 LoginFailure::FromNetworkAuthFailure(error));
  SetAttemptState(auth_, state_.release());

  RunResolve(auth_.get(), &message_loop_);
}

TEST_F(ParallelAuthenticatorTest, DriveOfflineLoginDelayedOnline) {
  ExpectLoginSuccess(username_, password_, result_, true);
  FailOnLoginFailure();

  // Set up state as though a cryptohome mount attempt has occurred and
  // succeeded.
  state_->PresetCryptohomeStatus(true, 0);
  // state_ is released further down.
  SetAttemptState(auth_, state_.get());
  RunResolve(auth_.get(), &message_loop_);

  // Offline login has completed, so now we "complete" the online request.
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  LoginFailure failure = LoginFailure::FromNetworkAuthFailure(error);
  state_.release()->PresetOnlineLoginStatus(result_, failure);
  ExpectLoginFailure(failure);

  RunResolve(auth_.get(), &message_loop_);
}

TEST_F(ParallelAuthenticatorTest, DriveOfflineLoginGetNewPassword) {
  ExpectLoginSuccess(username_, password_, result_, true);
  FailOnLoginFailure();

  // Set up mock cryptohome library to respond successfully to a key migration.
  mock_library_->SetUp(true, 0);
  EXPECT_CALL(*mock_library_, AsyncMigrateKey(username_,
                                              state_->ascii_hash,
                                              _,
                                              _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, HashPassword(_))
      .WillOnce(Return(std::string()))
      .RetiresOnSaturation();

  // Set up state as though a cryptohome mount attempt has occurred and
  // succeeded; also, an online request that never made it.
  state_->PresetCryptohomeStatus(true, 0);
  // state_ is released further down.
  SetAttemptState(auth_, state_.get());
  RunResolve(auth_.get(), &message_loop_);

  // Offline login has completed, so now we "complete" the online request.
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  LoginFailure failure = LoginFailure::FromNetworkAuthFailure(error);
  state_.release()->PresetOnlineLoginStatus(result_, failure);
  ExpectLoginFailure(failure);

  RunResolve(auth_.get(), &message_loop_);

  // After the request below completes, OnLoginSuccess gets called again.
  ExpectLoginSuccess(username_, password_, result_, false);

  MockFactory<SuccessFetcher> factory;
  TestingProfile profile;

  auth_->RetryAuth(&profile,
                   username_,
                   std::string(),
                   std::string(),
                   std::string());
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveOfflineLoginGetCaptchad) {
  ExpectLoginSuccess(username_, password_, result_, true);
  FailOnLoginFailure();
  EXPECT_CALL(*mock_library_, HashPassword(_))
      .WillOnce(Return(std::string()))
      .RetiresOnSaturation();

  // Set up state as though a cryptohome mount attempt has occurred and
  // succeeded; also, an online request that never made it.
  state_->PresetCryptohomeStatus(true, 0);
  // state_ is released further down.
  SetAttemptState(auth_, state_.get());
  RunResolve(auth_.get(), &message_loop_);

  // Offline login has completed, so now we "complete" the online request.
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  LoginFailure failure = LoginFailure::FromNetworkAuthFailure(error);
  state_.release()->PresetOnlineLoginStatus(result_, failure);
  ExpectLoginFailure(failure);

  RunResolve(auth_.get(), &message_loop_);

  // After the request below completes, OnLoginSuccess gets called again.
  failure = LoginFailure::FromNetworkAuthFailure(
      GoogleServiceAuthError::FromCaptchaChallenge(
          CaptchaFetcher::GetCaptchaToken(),
          GURL(CaptchaFetcher::GetCaptchaUrl()),
          GURL(CaptchaFetcher::GetUnlockUrl())));
  ExpectLoginFailure(failure);

  MockFactory<CaptchaFetcher> factory;
  TestingProfile profile;

  auth_->RetryAuth(&profile,
                   username_,
                   std::string(),
                   std::string(),
                   std::string());
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveOnlineLogin) {
  GaiaAuthConsumer::ClientLoginResult success("sid", "lsid", "", "");
  ExpectLoginSuccess(username_, password_, success, false);
  FailOnLoginFailure();

  // Set up state as though a cryptohome mount attempt has occurred and
  // succeeded.
  state_->PresetCryptohomeStatus(true, 0);
  state_->PresetOnlineLoginStatus(success, LoginFailure::None());
  SetAttemptState(auth_, state_.release());

  RunResolve(auth_.get(), &message_loop_);
}

TEST_F(ParallelAuthenticatorTest, FLAKY_DriveNeedNewPassword) {
  FailOnLoginSuccess();  // Set failing on success as the default...
  // ...but expect ONE successful login first.
  ExpectLoginSuccess(username_, password_, result_, true);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  LoginFailure failure = LoginFailure::FromNetworkAuthFailure(error);
  ExpectLoginFailure(failure);

  // Set up state as though a cryptohome mount attempt has occurred and
  // succeeded.
  state_->PresetCryptohomeStatus(true, 0);
  state_->PresetOnlineLoginStatus(result_, failure);
  SetAttemptState(auth_, state_.release());

  RunResolve(auth_.get(), &message_loop_);
}

TEST_F(ParallelAuthenticatorTest, DriveLocalLogin) {
  ExpectGuestLoginSuccess();
  FailOnLoginFailure();

  // Set up mock cryptohome library to respond as though a tmpfs mount
  // attempt has occurred and succeeded.
  mock_library_->SetUp(true, 0);
  EXPECT_CALL(*mock_library_, AsyncMountForBwsi(_))
      .Times(1)
      .RetiresOnSaturation();

  // Pre-set test state as though an online login attempt failed to complete,
  // and that a cryptohome mount attempt failed because the user doesn't exist.
  GoogleServiceAuthError error =
      GoogleServiceAuthError::FromConnectionError(net::ERR_CONNECTION_RESET);
  LoginFailure failure =
      LoginFailure::FromNetworkAuthFailure(error);
  state_->PresetOnlineLoginStatus(result_, failure);
  state_->PresetCryptohomeStatus(
      false,
      chromeos::kCryptohomeMountErrorUserDoesNotExist);
  SetAttemptState(auth_, state_.release());

  // Deal with getting the localaccount file
  FilePath tmp_file_path = FakeLocalaccountFile(username_);
  ReadLocalaccountFile(auth_.get(), tmp_file_path.BaseName().value());

  RunResolve(auth_.get(), &message_loop_);

  Delete(tmp_file_path, false);
}

TEST_F(ParallelAuthenticatorTest, DriveUnlock) {
  ExpectLoginSuccess(username_, std::string(), result_, false);
  FailOnLoginFailure();

  // Set up mock cryptohome library to respond successfully to a cryptohome
  // key-check attempt.
  mock_library_->SetUp(true, 0);
  EXPECT_CALL(*mock_library_, AsyncCheckKey(username_, _, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, HashPassword(_))
      .WillOnce(Return(std::string()))
      .RetiresOnSaturation();

  auth_->AuthenticateToUnlock(username_, "");
  message_loop_.Run();
}

TEST_F(ParallelAuthenticatorTest, DriveLocalUnlock) {
  ExpectLoginSuccess(username_, std::string(), result_, false);
  FailOnLoginFailure();

  // Set up mock cryptohome library to fail a cryptohome key-check
  // attempt.
  mock_library_->SetUp(false, 0);
  EXPECT_CALL(*mock_library_, AsyncCheckKey(username_, _, _))
      .Times(1)
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_library_, HashPassword(_))
      .WillOnce(Return(std::string()))
      .RetiresOnSaturation();

  // Deal with getting the localaccount file
  FilePath tmp_file_path = FakeLocalaccountFile(username_);
  ReadLocalaccountFile(auth_.get(), tmp_file_path.BaseName().value());

  auth_->AuthenticateToUnlock(username_, "");
  message_loop_.Run();

  Delete(tmp_file_path, false);
}

}  // namespace chromeos
