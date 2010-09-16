// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/nss_util.h"
#include "base/scoped_temp_dir.h"
#include "base/stringprintf.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/mock_owner_key_utils.h"
#include "chrome/browser/chromeos/login/mock_ownership_service.h"
#include "chrome/browser/chromeos/login/owner_manager_unittest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
        expected_(to_expect),
        run_(false) {}
  virtual ~DummyDelegate() { EXPECT_TRUE(run_); }
  virtual void OnSettingsOpSucceeded(T value) {
    run_ = true;
    EXPECT_TRUE(expect_success_);
    EXPECT_EQ(expected_, value);
  }
  virtual void OnSettingsOpFailed() {
    run_ = true;
    EXPECT_FALSE(expect_success_);
  }
  virtual void expect_success() { expect_success_ = true; }
  bool expect_success_;
  T expected_;
  bool run_;
};

class Quitter : public DummyDelegate<bool> {
 public:
  explicit Quitter(bool to_expect) : DummyDelegate<bool>(to_expect) {}
  virtual ~Quitter() {}
  void OnSettingsOpSucceeded(bool value) {
    DummyDelegate<bool>::OnSettingsOpSucceeded(value);
    MessageLoop::current()->Quit();
  }
  void OnSettingsOpFailed() {
    DummyDelegate<bool>::OnSettingsOpFailed();
    MessageLoop::current()->Quit();
  }
};
}  // anonymous namespace

class SignedSettingsTest : public ::testing::Test {
 public:
  SignedSettingsTest()
      : fake_email_("fakey"),
        fake_prop_("prop_name"),
        fake_value_("stub"),
        message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(ChromeThread::UI, &message_loop_),
        file_thread_(ChromeThread::FILE),
        mock_(new MockKeyUtils),
        injector_(mock_) /* injector_ takes ownership of mock_ */ {
  }

  virtual ~SignedSettingsTest() {}

  virtual void SetUp() {
    chromeos::CrosLibrary::Get()->GetTestApi()->SetUseStubImpl();
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

    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, StartVerifyAttempt(fake_email_, _, _))
        .Times(1);

    EXPECT_TRUE(s->Execute());
    s->OnKeyOpComplete(return_code, std::vector<uint8>());
  }

  void FailingWhitelistOp(const OwnerManager::KeyOpCode return_code) {
    DummyDelegate<bool> d(false);
    scoped_refptr<SignedSettings> s(
        SignedSettings::CreateWhitelistOp(fake_email_, true, &d));

    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, StartSigningAttempt(fake_email_, _))
        .Times(1);

    EXPECT_TRUE(s->Execute());
    s->OnKeyOpComplete(return_code, std::vector<uint8>());
  }

  void FailingStorePropertyOp(const OwnerManager::KeyOpCode return_code) {
    DummyDelegate<bool> d(false);
    scoped_refptr<SignedSettings> s(
        SignedSettings::CreateStorePropertyOp(fake_prop_, fake_value_, &d));
    std::string to_sign = base::StringPrintf("%s=%s",
                                             fake_prop_.c_str(),
                                             fake_value_.c_str());
    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, StartSigningAttempt(to_sign, _))
        .Times(1);

    EXPECT_TRUE(s->Execute());
    s->OnKeyOpComplete(return_code, std::vector<uint8>());
  }

  void FailingRetrievePropertyOp(const OwnerManager::KeyOpCode return_code) {
    DummyDelegate<std::string> d(fake_value_);
    scoped_refptr<SignedSettings> s(
        SignedSettings::CreateRetrievePropertyOp(fake_prop_, &d));
    std::string to_verify = base::StringPrintf("%s=%s",
                                               fake_prop_.c_str(),
                                               fake_value_.c_str());
    mock_service(s.get(), &m_);
    EXPECT_CALL(m_, StartVerifyAttempt(to_verify, _, _))
        .Times(1);

    EXPECT_TRUE(s->Execute());
    s->OnKeyOpComplete(return_code, std::vector<uint8>());
  }

  const std::string fake_email_;
  const std::string fake_prop_;
  const std::string fake_value_;
  MockOwnershipService m_;

  ScopedTempDir tmpdir_;
  FilePath tmpfile_;

  MessageLoop message_loop_;
  ChromeThread ui_thread_;
  ChromeThread file_thread_;

  std::vector<uint8> fake_public_key_;
  scoped_ptr<RSAPrivateKey> fake_private_key_;

  MockKeyUtils* mock_;
  MockInjector injector_;

};

TEST_F(SignedSettingsTest, CheckWhitelist) {
  DummyDelegate<bool> d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateCheckWhitelistOp(fake_email_, &d));

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartVerifyAttempt(fake_email_, _, _))
      .Times(1);

  EXPECT_TRUE(s->Execute());
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
}

TEST_F(SignedSettingsTest, CheckWhitelistNoKey) {
  FailingCheckWhitelist(OwnerManager::KEY_UNAVAILABLE);
}

TEST_F(SignedSettingsTest, CheckWhitelistFailed) {
  FailingCheckWhitelist(OwnerManager::OPERATION_FAILED);
}

TEST_F(SignedSettingsTest, Whitelist) {
  Quitter d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateWhitelistOp(fake_email_, true, &d));

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartSigningAttempt(fake_email_, _))
      .Times(1);

  EXPECT_TRUE(s->Execute());
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.Run();
}

TEST_F(SignedSettingsTest, Unwhitelist) {
  Quitter d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateWhitelistOp(fake_email_, false, &d));

  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartSigningAttempt(fake_email_, _))
      .Times(1);

  EXPECT_TRUE(s->Execute());
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.Run();
}

TEST_F(SignedSettingsTest, WhitelistNoKey) {
  FailingWhitelistOp(OwnerManager::KEY_UNAVAILABLE);
}

TEST_F(SignedSettingsTest, WhitelistFailed) {
  FailingWhitelistOp(OwnerManager::OPERATION_FAILED);
}

TEST_F(SignedSettingsTest, StoreProperty) {
  Quitter d(true);
  d.expect_success();
  scoped_refptr<SignedSettings> s(
      SignedSettings::CreateStorePropertyOp(fake_prop_, fake_value_, &d));
  std::string to_sign = base::StringPrintf("%s=%s",
                                           fake_prop_.c_str(),
                                           fake_value_.c_str());
  mock_service(s.get(), &m_);
  EXPECT_CALL(m_, StartSigningAttempt(to_sign, _))
      .Times(1);

  EXPECT_TRUE(s->Execute());
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  message_loop_.Run();
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

  EXPECT_TRUE(s->Execute());
  s->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
}

TEST_F(SignedSettingsTest, RetrievePropertyNoKey) {
  FailingRetrievePropertyOp(OwnerManager::KEY_UNAVAILABLE);
}

TEST_F(SignedSettingsTest, RetrievePropertyFailed) {
  FailingRetrievePropertyOp(OwnerManager::OPERATION_FAILED);
}

}  // namespace chromeos
