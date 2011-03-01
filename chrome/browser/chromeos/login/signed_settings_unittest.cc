// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/nss_util.h"
#include "base/scoped_temp_dir.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/cros/mock_login_library.h"
#include "chrome/browser/chromeos/login/mock_owner_key_utils.h"
#include "chrome/browser/chromeos/login/mock_ownership_service.h"
#include "chrome/browser/chromeos/login/owner_manager_unittest.h"
#include "chrome/test/thread_test_helper.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

namespace chromeos {

namespace {
template <class T>
class DummyDelegate : public SignedSettings::Delegate<T> {
 public:
  explicit DummyDelegate(T to_expect)
      : expect_success_(false),
        expected_failure_(SignedSettings::SUCCESS),
        expected_(to_expect),
        run_(false) {}
  virtual ~DummyDelegate() { EXPECT_TRUE(run_); }
  virtual void OnSettingsOpCompleted(SignedSettings::ReturnCode code,
                                     T value) {
    run_ = true;
    if (expect_success_)
      EXPECT_EQ(expected_, value);
    EXPECT_EQ(expected_failure_, code);
  }
  virtual void expect_success() {
    expect_success_ = true;
    expected_failure_ = SignedSettings::SUCCESS;
  }
  virtual void expect_failure(SignedSettings::ReturnCode code) {
    expect_success_ = false;
    expected_failure_ = code;
  }
  bool expect_success_;
  SignedSettings::ReturnCode expected_failure_;
  T expected_;
  bool run_;
};

}  // anonymous namespace

class SignedSettingsTest : public ::testing::Test {
 public:
  SignedSettingsTest()
      : fake_email_("fakey@example.com"),
        fake_prop_("prop_name"),
        fake_value_("stub"),
        message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE),
        mock_(new MockKeyUtils),
        injector_(mock_) /* injector_ takes ownership of mock_ */ {
  }

  virtual ~SignedSettingsTest() {}

  virtual void SetUp() {
    file_thread_.Start();
  }

  virtual void TearDown() {
    OwnerKeyUtils::set_factory(NULL);
  }

  void mock_service(SignedSettings* s, MockOwnershipService* m) {
    s->set_service(m);
  }

  void FailingCheckWhitelist(const OwnerManager::KeyOpCode return_code) {
    DummyDelegate<bool> d(false);
    scoped_refptr<SignedSettings> s(
        SignedSettings::CreateCheckWhitelistOp(fake_email_, &d));
    d.expect_failure(SignedSettings::MapKeyOpCode(return_code));

    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, StartVerifyAttempt(fake_email_, _, _))
        .Times(1);

    s->Execute();
    s->OnKeyOpComplete(return_code, std::vector<uint8>());
  }

  void FailingWhitelistOp(const OwnerManager::KeyOpCode return_code) {
    DummyDelegate<bool> d(false);
    scoped_refptr<SignedSettings> s(
        SignedSettings::CreateWhitelistOp(fake_email_, true, &d));
    d.expect_failure(SignedSettings::MapKeyOpCode(return_code));

    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, StartSigningAttempt(fake_email_, _))
        .Times(1);

    s->Execute();
    s->OnKeyOpComplete(return_code, std::vector<uint8>());
  }

  void FailingStorePropertyOp(const OwnerManager::KeyOpCode return_code) {
    DummyDelegate<bool> d(false);
    scoped_refptr<SignedSettings> s(
        SignedSettings::CreateStorePropertyOp(fake_prop_, fake_value_, &d));
    d.expect_failure(SignedSettings::MapKeyOpCode(return_code));
    std::string to_sign = base::StringPrintf("%s=%s",
                                             fake_prop_.c_str(),
                                             fake_value_.c_str());
    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, StartSigningAttempt(to_sign, _))
        .Times(1);
    EXPECT_CALL(m_, GetStatus(_))
        .WillOnce(Return(OwnershipService::OWNERSHIP_TAKEN));

    s->Execute();
    s->OnKeyOpComplete(return_code, std::vector<uint8>());
  }

  void FailingRetrievePropertyOp(const OwnerManager::KeyOpCode return_code) {
    DummyDelegate<std::string> d(fake_value_);
    scoped_refptr<SignedSettings> s(
        SignedSettings::CreateRetrievePropertyOp(fake_prop_, &d));
    d.expect_failure(SignedSettings::MapKeyOpCode(return_code));
    std::string to_verify = base::StringPrintf("%s=%s",
                                               fake_prop_.c_str(),
                                               fake_value_.c_str());
    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, StartVerifyAttempt(to_verify, _, _))
        .Times(1);
    EXPECT_CALL(m_, GetStatus(_))
        .WillOnce(Return(OwnershipService::OWNERSHIP_TAKEN));

    s->Execute();
    s->OnKeyOpComplete(return_code, std::vector<uint8>());
  }

  MockLoginLibrary* MockLoginLib() {
    chromeos::CrosLibrary::TestApi* test_api =
        chromeos::CrosLibrary::Get()->GetTestApi();

    // Mocks, ownership transferred to CrosLibrary class on creation.
    MockLoginLibrary* mock_library;
    MockLibraryLoader* loader;

    loader = new MockLibraryLoader();
    ON_CALL(*loader, Load(_))
        .WillByDefault(Return(true));
    EXPECT_CALL(*loader, Load(_))
        .Times(AnyNumber());

    test_api->SetLibraryLoader(loader, true);

    mock_library = new MockLoginLibrary();
    test_api->SetLoginLibrary(mock_library, true);
    return mock_library;
  }

  void UnMockLoginLib() {
    // Prevent bogus gMock leak check from firing.
    chromeos::CrosLibrary::TestApi* test_api =
        chromeos::CrosLibrary::Get()->GetTestApi();
    test_api->SetLibraryLoader(NULL, false);
    test_api->SetLoginLibrary(NULL, false);
  }

  const std::string fake_email_;
  const std::string fake_prop_;
  const std::string fake_value_;
  MockOwnershipService m_;

  ScopedTempDir tmpdir_;
  FilePath tmpfile_;

  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;

  std::vector<uint8> fake_public_key_;
  scoped_ptr<RSAPrivateKey> fake_private_key_;

  MockKeyUtils* mock_;
  MockInjector injector_;

  ScopedStubCrosEnabler stub_cros_enabler_;
};

TEST_F(SignedSettingsTest, CheckWhitelist) {
  DummyDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateCheckWhitelistOp(fake_email_, &d));

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartVerifyAttempt(fake_email_, _, _))
      .Times(1);

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
}

TEST_F(SignedSettingsTest, CheckWhitelistNotFound) {
  DummyDelegate<bool> d(true);
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateCheckWhitelistOp(fake_email_, &d));
  d.expect_failure(SignedSettings::NOT_FOUND);
  MockLoginLibrary* lib = MockLoginLib();
  EXPECT_CALL(*lib, CheckWhitelist(fake_email_, _))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  s->Execute();
  UnMockLoginLib();
}

TEST_F(SignedSettingsTest, CheckWhitelistNoKey) {
  FailingCheckWhitelist(OwnerManager::KEY_UNAVAILABLE);
}

TEST_F(SignedSettingsTest, CheckWhitelistFailed) {
  FailingCheckWhitelist(OwnerManager::OPERATION_FAILED);
}

TEST_F(SignedSettingsTest, Whitelist) {
  DummyDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateWhitelistOp(fake_email_, true, &d));

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartSigningAttempt(fake_email_, _))
      .Times(1);

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, Unwhitelist) {
  DummyDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateWhitelistOp(fake_email_, false, &d));

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartSigningAttempt(fake_email_, _))
      .Times(1);

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, WhitelistNoKey) {
  FailingWhitelistOp(OwnerManager::KEY_UNAVAILABLE);
}

TEST_F(SignedSettingsTest, WhitelistFailed) {
  FailingWhitelistOp(OwnerManager::OPERATION_FAILED);
}

TEST_F(SignedSettingsTest, StoreProperty) {
  DummyDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateStorePropertyOp(fake_prop_, fake_value_, &d));
  std::string to_sign = base::StringPrintf("%s=%s",
                                           fake_prop_.c_str(),
                                           fake_value_.c_str());
  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartSigningAttempt(to_sign, _))
      .Times(1);
  EXPECT_CALL(m_, GetStatus(_))
      .WillOnce(Return(OwnershipService::OWNERSHIP_TAKEN));

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.RunAllPending();
}

TEST_F(SignedSettingsTest, StorePropertyNoKey) {
  FailingStorePropertyOp(OwnerManager::KEY_UNAVAILABLE);
}

TEST_F(SignedSettingsTest, StorePropertyFailed) {
  FailingStorePropertyOp(OwnerManager::OPERATION_FAILED);
}

TEST_F(SignedSettingsTest, RetrieveProperty) {
  DummyDelegate<std::string> d(fake_value_);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateRetrievePropertyOp(fake_prop_, &d));
  std::string to_verify = base::StringPrintf("%s=%s",
                                             fake_prop_.c_str(),
                                             fake_value_.c_str());
  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartVerifyAttempt(to_verify, _, _))
      .Times(1);
  EXPECT_CALL(m_, GetStatus(_))
      .WillOnce(Return(OwnershipService::OWNERSHIP_TAKEN));

  s->Execute();
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
}

TEST_F(SignedSettingsTest, RetrievePropertyNotFound) {
  DummyDelegate<std::string> d(fake_value_);
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateRetrievePropertyOp(fake_prop_, &d));
  d.expect_failure(SignedSettings::NOT_FOUND);
  MockLoginLibrary* lib = MockLoginLib();
  EXPECT_CALL(*lib, RetrieveProperty(fake_prop_, _, _))
      .WillOnce(Return(false))
      .RetiresOnSaturation();
  s->Execute();
  UnMockLoginLib();
}

TEST_F(SignedSettingsTest, RetrievePropertyNoKey) {
  FailingRetrievePropertyOp(OwnerManager::KEY_UNAVAILABLE);
}

TEST_F(SignedSettingsTest, RetrievePropertyFailed) {
  FailingRetrievePropertyOp(OwnerManager::OPERATION_FAILED);
}

}  // namespace chromeos
