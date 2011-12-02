// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/signed_settings_helper.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/mock_dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/mock_session_manager_client.h"
#include "chrome/browser/chromeos/login/mock_owner_key_utils.h"
#include "chrome/browser/chromeos/login/mock_ownership_service.h"
#include "chrome/browser/chromeos/login/owner_manager.h"
#include "chrome/browser/chromeos/login/signed_settings.h"
#include "chrome/browser/policy/proto/chrome_device_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::A;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::WithArg;

namespace em = enterprise_management;
namespace chromeos {

ACTION_P(Retrieve, policy_blob) { arg0.Run(policy_blob); }
ACTION_P(Store, success) { arg1.Run(success); }

class SignedSettingsHelperTest : public testing::Test,
                                 public SignedSettingsHelper::TestDelegate {
 public:
  SignedSettingsHelperTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        pending_ops_(0),
        mock_dbus_thread_manager_(new MockDBusThreadManager) {
  }

  virtual void SetUp() {
    SignedSettingsHelper::Get()->set_test_delegate(this);
    DBusThreadManager::InitializeForTesting(mock_dbus_thread_manager_);

    fake_policy_data_ = BuildPolicyData();
    std::string data_serialized = fake_policy_data_.SerializeAsString();
    std::string serialized_policy_;
    fake_policy_ = BuildProto(data_serialized,
                              std::string("false"),
                              &serialized_policy_);

    MockSessionManagerClient* client =
        mock_dbus_thread_manager_->mock_session_manager_client();
    // Make sure the mocked out class calls back to notify success on store and
    // retrieve ops.
    EXPECT_CALL(*client, StorePolicy(_, _))
        .WillRepeatedly(Store(true));
    EXPECT_CALL(*client, RetrievePolicy(_))
        .WillRepeatedly(Retrieve(serialized_policy_));

    EXPECT_CALL(m_, StartSigningAttempt(_, A<OwnerManager::Delegate*>()))
        .WillRepeatedly(WithArg<1>(
            Invoke(&SignedSettingsHelperTest::OnKeyOpComplete)));
    EXPECT_CALL(m_, StartVerifyAttempt(_, _, A<OwnerManager::Delegate*>()))
        .WillRepeatedly(WithArg<2>(
            Invoke(&SignedSettingsHelperTest::OnKeyOpComplete)));
  }

  virtual void TearDown() {
    DBusThreadManager::Shutdown();
    SignedSettingsHelper::Get()->set_test_delegate(NULL);
  }

  virtual void OnOpCreated(SignedSettings* op) {
    // Use MockOwnershipService for all SignedSettings op.
    op->set_service(&m_);
  }

  virtual void OnOpStarted(SignedSettings* op) {
  }

  virtual void OnOpCompleted(SignedSettings* op) {
    --pending_ops_;
  }

  static void OnKeyOpComplete(OwnerManager::Delegate* op) {
    op->OnKeyOpComplete(OwnerManager::SUCCESS, std::vector<uint8>());
  }

  em::PolicyData BuildPolicyData() {
    em::PolicyData to_return;
    em::ChromeDeviceSettingsProto pol;
    to_return.set_policy_type(chromeos::kDevicePolicyType);
    to_return.set_policy_value(pol.SerializeAsString());
    return to_return;
  }

  em::PolicyFetchResponse BuildProto(const std::string& data,
                                     const std::string& sig,
                                     std::string* out_serialized) {
    em::PolicyFetchResponse fake_policy;
    if (!data.empty())
      fake_policy.set_policy_data(data);
    if (!sig.empty())
      fake_policy.set_policy_data_signature(sig);
    EXPECT_TRUE(fake_policy.SerializeToString(out_serialized));
    return fake_policy;
  }

  em::PolicyData fake_policy_data_;
  em::PolicyFetchResponse fake_policy_;
  std::string serialized_policy_;
  MockOwnershipService m_;

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  int pending_ops_;

  MockDBusThreadManager* mock_dbus_thread_manager_;

  ScopedStubCrosEnabler stub_cros_enabler_;
};

class SignedSettingsCallbacks {
 public:
  virtual ~SignedSettingsCallbacks() {}
  // Callback of StorePolicyOp.
  virtual void OnStorePolicyCompleted(SignedSettings::ReturnCode code) = 0;
  // Callback of RetrievePolicyOp.
  virtual void OnRetrievePolicyCompleted(
      SignedSettings::ReturnCode code,
      const em::PolicyFetchResponse& policy) = 0;
};

class MockSignedSettingsCallbacks
    : public SignedSettingsCallbacks,
      public base::SupportsWeakPtr<MockSignedSettingsCallbacks> {
public:
  virtual ~MockSignedSettingsCallbacks() {}

  MOCK_METHOD1(OnStorePolicyCompleted, void(SignedSettings::ReturnCode));
  MOCK_METHOD2(OnRetrievePolicyCompleted, void(SignedSettings::ReturnCode,
                                               const em::PolicyFetchResponse&));
};

TEST_F(SignedSettingsHelperTest, SerializedOps) {
  MockSignedSettingsCallbacks cb;

  InSequence s;
  EXPECT_CALL(cb, OnStorePolicyCompleted(SignedSettings::SUCCESS))
      .Times(1);
  EXPECT_CALL(cb, OnRetrievePolicyCompleted(SignedSettings::SUCCESS, _))
      .Times(1);


  pending_ops_ = 2;
  SignedSettingsHelper::Get()->StartStorePolicyOp(
      fake_policy_,
      base::Bind(&MockSignedSettingsCallbacks::OnStorePolicyCompleted,
                 base::Unretained(&cb)));
  SignedSettingsHelper::Get()->StartRetrievePolicyOp(
      base::Bind(&MockSignedSettingsCallbacks::OnRetrievePolicyCompleted,
                 base::Unretained(&cb)));

  message_loop_.RunAllPending();
  ASSERT_EQ(0, pending_ops_);
}

TEST_F(SignedSettingsHelperTest, CanceledOps) {
  MockSignedSettingsCallbacks cb;

  InSequence s;
  EXPECT_CALL(cb, OnStorePolicyCompleted(SignedSettings::SUCCESS))
      .Times(1);
  EXPECT_CALL(cb, OnRetrievePolicyCompleted(SignedSettings::SUCCESS, _))
      .Times(1);

  pending_ops_ = 3;

  {
    // This op will be deleted and never will be executed (expect only one call
    // to OnRetrievePolicyCompleted above). However the OpComplete callback will
    // be still called, therefore we expect three pending ops.
    MockSignedSettingsCallbacks cb_to_be_deleted;
    SignedSettingsHelper::Get()->StartRetrievePolicyOp(
        base::Bind(&MockSignedSettingsCallbacks::OnRetrievePolicyCompleted,
                   cb_to_be_deleted.AsWeakPtr()));
  }
  SignedSettingsHelper::Get()->StartStorePolicyOp(
      fake_policy_,
      base::Bind(&MockSignedSettingsCallbacks::OnStorePolicyCompleted,
                 base::Unretained(&cb)));
  SignedSettingsHelper::Get()->StartRetrievePolicyOp(
      base::Bind(&MockSignedSettingsCallbacks::OnRetrievePolicyCompleted,
                 base::Unretained(&cb)));

  message_loop_.RunAllPending();
  ASSERT_EQ(0, pending_ops_);
}

}  // namespace chromeos
