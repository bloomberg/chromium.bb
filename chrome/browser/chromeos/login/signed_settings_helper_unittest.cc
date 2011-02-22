// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings_helper.h"

#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/mock_ownership_service.h"
#include "chrome/browser/chromeos/login/owner_manager.h"
#include "chrome/browser/chromeos/login/signed_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;

namespace chromeos {

class MockSignedSettingsHelperCallback : public SignedSettingsHelper::Callback {
 public:
  MOCK_METHOD2(OnCheckWhitelistCompleted, void(
      SignedSettings::ReturnCode code, const std::string& email));
  MOCK_METHOD2(OnWhitelistCompleted, void(
      SignedSettings::ReturnCode code, const std::string& email));
  MOCK_METHOD2(OnUnwhitelistCompleted, void(
      SignedSettings::ReturnCode code, const std::string& email));
  MOCK_METHOD3(OnStorePropertyCompleted, void(
      SignedSettings::ReturnCode code,
      const std::string& name,
      const std::string& value));
  MOCK_METHOD3(OnRetrievePropertyCompleted, void(
      SignedSettings::ReturnCode code,
      const std::string& name,
      const std::string& value));
};

class SignedSettingsHelperTest : public ::testing::Test,
                                 public SignedSettingsHelper::TestDelegate {
 public:
  SignedSettingsHelperTest()
      : fake_email_("fakey@example.com"),
        fake_prop_("prop_name"),
        fake_value_("stub"),
        message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE),
        pending_ops_(0) {
  }

  virtual void SetUp() {
    file_thread_.Start();
    SignedSettingsHelper::Get()->set_test_delegate(this);
  }

  virtual void TearDown() {
    SignedSettingsHelper::Get()->set_test_delegate(NULL);
  }

  virtual void OnOpCreated(SignedSettings* op) {
    // Use MockOwnershipService for all SignedSettings op.
    op->set_service(&m_);
  }

  virtual void OnOpStarted(SignedSettings* op) {
    op->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  }

  virtual void OnOpCompleted(SignedSettings* op) {
    --pending_ops_;
    if (!pending_ops_)
      MessageLoop::current()->Quit();
  }

  const std::string fake_email_;
  const std::string fake_prop_;
  const std::string fake_value_;
  MockOwnershipService m_;

  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;

  int pending_ops_;

  ScopedStubCrosEnabler stub_cros_enabler_;
};

TEST_F(SignedSettingsHelperTest, SerializedOps) {
  MockSignedSettingsHelperCallback cb;

  ON_CALL(m_, GetStatus(_))
      .WillByDefault(Return(OwnershipService::OWNERSHIP_TAKEN));
  EXPECT_CALL(m_, GetStatus(_)).Times(2);
  InSequence s;
  EXPECT_CALL(m_, StartVerifyAttempt(_, _, _)).Times(1);
  EXPECT_CALL(cb, OnCheckWhitelistCompleted(SignedSettings::SUCCESS, _))
      .Times(1);
  EXPECT_CALL(m_, StartSigningAttempt(_, _)).Times(1);
  EXPECT_CALL(cb, OnWhitelistCompleted(SignedSettings::SUCCESS, _))
      .Times(1);
  EXPECT_CALL(m_, StartSigningAttempt(_, _)).Times(1);
  EXPECT_CALL(cb, OnUnwhitelistCompleted(SignedSettings::SUCCESS, _))
      .Times(1);
  EXPECT_CALL(m_, StartSigningAttempt(_, _)).Times(1);
  EXPECT_CALL(cb, OnStorePropertyCompleted(SignedSettings::SUCCESS, _, _))
      .Times(1);
  EXPECT_CALL(m_, StartVerifyAttempt(_, _, _)).Times(1);
  EXPECT_CALL(cb, OnRetrievePropertyCompleted(SignedSettings::SUCCESS, _, _))
      .Times(1);

  pending_ops_ = 5;
  SignedSettingsHelper::Get()->StartCheckWhitelistOp(fake_email_, &cb);
  SignedSettingsHelper::Get()->StartWhitelistOp(fake_email_, true, &cb);
  SignedSettingsHelper::Get()->StartWhitelistOp(fake_email_, false, &cb);
  SignedSettingsHelper::Get()->StartStorePropertyOp(fake_prop_, fake_value_,
      &cb);
  SignedSettingsHelper::Get()->StartRetrieveProperty(fake_prop_, &cb);

  message_loop_.Run();
}

TEST_F(SignedSettingsHelperTest, CanceledOps) {
  MockSignedSettingsHelperCallback cb;

  ON_CALL(m_, GetStatus(_))
      .WillByDefault(Return(OwnershipService::OWNERSHIP_TAKEN));
  EXPECT_CALL(m_, GetStatus(_)).Times(2);
  InSequence s;
  EXPECT_CALL(m_, StartVerifyAttempt(_, _, _)).Times(1);
  EXPECT_CALL(cb, OnCheckWhitelistCompleted(SignedSettings::SUCCESS, _))
      .Times(1);
  EXPECT_CALL(m_, StartSigningAttempt(_, _)).Times(1);
  EXPECT_CALL(cb, OnWhitelistCompleted(SignedSettings::SUCCESS, _))
      .Times(1);
  EXPECT_CALL(m_, StartSigningAttempt(_, _)).Times(1);
  EXPECT_CALL(cb, OnUnwhitelistCompleted(SignedSettings::SUCCESS, _))
      .Times(1);

  // CheckWhitelistOp for cb_to_be_canceled still gets executed but callback
  // does not happen.
  EXPECT_CALL(m_, StartVerifyAttempt(_, _, _)).Times(1);

  EXPECT_CALL(m_, StartSigningAttempt(_, _)).Times(1);
  EXPECT_CALL(cb, OnStorePropertyCompleted(SignedSettings::SUCCESS, _, _))
      .Times(1);
  EXPECT_CALL(m_, StartVerifyAttempt(_, _, _)).Times(1);
  EXPECT_CALL(cb, OnRetrievePropertyCompleted(SignedSettings::SUCCESS, _, _))
      .Times(1);

  pending_ops_ = 6;
  SignedSettingsHelper::Get()->StartCheckWhitelistOp(fake_email_, &cb);
  SignedSettingsHelper::Get()->StartWhitelistOp(fake_email_, true, &cb);
  SignedSettingsHelper::Get()->StartWhitelistOp(fake_email_, false, &cb);

  MockSignedSettingsHelperCallback cb_to_be_canceled;
  SignedSettingsHelper::Get()->StartCheckWhitelistOp(fake_email_,
      &cb_to_be_canceled);
  SignedSettingsHelper::Get()->CancelCallback(&cb_to_be_canceled);

  SignedSettingsHelper::Get()->StartStorePropertyOp(fake_prop_, fake_value_,
      &cb);
  SignedSettingsHelper::Get()->StartRetrieveProperty(fake_prop_, &cb);

  message_loop_.Run();
}

}  // namespace chromeos
