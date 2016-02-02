// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_management_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::NiceMock;
using testing::_;

namespace em = enterprise_management;

namespace {
const char kAttributeOwnerId[] = "consumer_management.owner_id";
const char kTestOwner[] = "test@chromium.org.test";
}

namespace policy {

class ConsumerManagementServiceTest : public BrowserWithTestWindowTest {
 public:
  ConsumerManagementServiceTest()
      : cryptohome_result_(false),
        set_owner_status_(false) {
    ON_CALL(mock_cryptohome_client_, GetBootAttribute(_, _))
        .WillByDefault(
            Invoke(this, &ConsumerManagementServiceTest::MockGetBootAttribute));
    ON_CALL(mock_cryptohome_client_, SetBootAttribute(_, _))
        .WillByDefault(
            Invoke(this, &ConsumerManagementServiceTest::MockSetBootAttribute));
    ON_CALL(mock_cryptohome_client_, FlushAndSignBootAttributes(_, _))
        .WillByDefault(
            Invoke(this,
                   &ConsumerManagementServiceTest::
                       MockFlushAndSignBootAttributes));
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    testing_profile_manager_.reset(new TestingProfileManager(
        TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    service_.reset(new ConsumerManagementService(&mock_cryptohome_client_,
                                                 NULL));
  }

  void TearDown() override {
    testing_profile_manager_.reset();
    service_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  ConsumerManagementStage GetStageFromLocalState() {
    return ConsumerManagementStage::FromInternalValue(
        g_browser_process->local_state()->GetInteger(
            prefs::kConsumerManagementStage));
  }

  void SetStageInLocalState(ConsumerManagementStage stage) {
    g_browser_process->local_state()->SetInteger(
        prefs::kConsumerManagementStage, stage.ToInternalValue());
  }

  void MockGetBootAttribute(
      const cryptohome::GetBootAttributeRequest& request,
      const chromeos::CryptohomeClient::ProtobufMethodCallback& callback) {
    get_boot_attribute_request_ = request;
    callback.Run(cryptohome_status_, cryptohome_result_, cryptohome_reply_);
  }

  void MockSetBootAttribute(
      const cryptohome::SetBootAttributeRequest& request,
      const chromeos::CryptohomeClient::ProtobufMethodCallback& callback) {
    set_boot_attribute_request_ = request;
    callback.Run(cryptohome_status_, cryptohome_result_, cryptohome_reply_);
  }

  void MockFlushAndSignBootAttributes(
      const cryptohome::FlushAndSignBootAttributesRequest& request,
      const chromeos::CryptohomeClient::ProtobufMethodCallback& callback) {
    callback.Run(cryptohome_status_, cryptohome_result_, cryptohome_reply_);
  }

  void OnGetOwnerDone(const std::string& owner) {
    owner_ = owner;
  }

  void OnSetOwnerDone(bool status) {
    set_owner_status_ = status;
  }

  NiceMock<chromeos::MockCryptohomeClient> mock_cryptohome_client_;
  scoped_ptr<ConsumerManagementService> service_;
  scoped_ptr<TestingProfileManager> testing_profile_manager_;

  // Variables for setting the return value or catching the arguments of mock
  // functions.
  chromeos::DBusMethodCallStatus cryptohome_status_;
  bool cryptohome_result_;
  cryptohome::BaseReply cryptohome_reply_;
  cryptohome::GetBootAttributeRequest get_boot_attribute_request_;
  cryptohome::SetBootAttributeRequest set_boot_attribute_request_;
  std::string owner_;
  bool set_owner_status_;
};

TEST_F(ConsumerManagementServiceTest, CanGetStage) {
  EXPECT_EQ(ConsumerManagementStage::None(), service_->GetStage());

  SetStageInLocalState(ConsumerManagementStage::EnrollmentRequested());

  EXPECT_EQ(ConsumerManagementStage::EnrollmentRequested(),
            service_->GetStage());
}

TEST_F(ConsumerManagementServiceTest, CanSetStage) {
  EXPECT_EQ(ConsumerManagementStage::None(), GetStageFromLocalState());

  service_->SetStage(ConsumerManagementStage::EnrollmentRequested());

  EXPECT_EQ(ConsumerManagementStage::EnrollmentRequested(),
            GetStageFromLocalState());
}

TEST_F(ConsumerManagementServiceTest, CanGetOwner) {
  cryptohome_status_ = chromeos::DBUS_METHOD_CALL_SUCCESS;
  cryptohome_result_ = true;
  cryptohome_reply_.MutableExtension(cryptohome::GetBootAttributeReply::reply)->
      set_value(kTestOwner);

  service_->GetOwner(base::Bind(&ConsumerManagementServiceTest::OnGetOwnerDone,
                                base::Unretained(this)));

  EXPECT_EQ(kAttributeOwnerId, get_boot_attribute_request_.name());
  EXPECT_EQ(kTestOwner, owner_);
}

TEST_F(ConsumerManagementServiceTest, GetOwnerReturnsAnEmptyStringWhenItFails) {
  cryptohome_status_ = chromeos::DBUS_METHOD_CALL_FAILURE;
  cryptohome_result_ = false;
  cryptohome_reply_.MutableExtension(cryptohome::GetBootAttributeReply::reply)->
      set_value(kTestOwner);

  service_->GetOwner(base::Bind(&ConsumerManagementServiceTest::OnGetOwnerDone,
                                base::Unretained(this)));

  EXPECT_EQ("", owner_);
}

TEST_F(ConsumerManagementServiceTest, CanSetOwner) {
  cryptohome_status_ = chromeos::DBUS_METHOD_CALL_SUCCESS;
  cryptohome_result_ = true;

  service_->SetOwner(kTestOwner,
                     base::Bind(&ConsumerManagementServiceTest::OnSetOwnerDone,
                                base::Unretained(this)));

  EXPECT_EQ(kAttributeOwnerId, set_boot_attribute_request_.name());
  EXPECT_EQ(kTestOwner, set_boot_attribute_request_.value());
  EXPECT_TRUE(set_owner_status_);
}

TEST_F(ConsumerManagementServiceTest, SetOwnerReturnsFalseWhenItFails) {
  cryptohome_status_ = chromeos::DBUS_METHOD_CALL_FAILURE;
  cryptohome_result_ = false;

  service_->SetOwner(kTestOwner,
                     base::Bind(&ConsumerManagementServiceTest::OnSetOwnerDone,
                                base::Unretained(this)));

  EXPECT_FALSE(set_owner_status_);
}

class ConsumerManagementServiceStatusTest
    : public chromeos::DeviceSettingsTestBase {
 public:
  ConsumerManagementServiceStatusTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()),
        service_(NULL, &device_settings_service_) {
  }

  void SetStageInLocalState(ConsumerManagementStage stage) {
    testing_local_state_.Get()->SetInteger(
        prefs::kConsumerManagementStage, stage.ToInternalValue());
  }

  void SetManagementMode(em::PolicyData::ManagementMode mode) {
    device_policy_.policy_data().set_management_mode(mode);
    device_policy_.Build();
    device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
    ReloadDeviceSettings();
  }

  ScopedTestingLocalState testing_local_state_;
  ConsumerManagementService service_;
};

TEST_F(ConsumerManagementServiceStatusTest,
       GetStatusAndGetStatusStringNotificationWork) {
  EXPECT_EQ(ConsumerManagementService::STATUS_UNKNOWN, service_.GetStatus());
  EXPECT_EQ("StatusUnknown", service_.GetStatusString());

  SetManagementMode(em::PolicyData::LOCAL_OWNER);
  SetStageInLocalState(ConsumerManagementStage::None());

  EXPECT_EQ(ConsumerManagementService::STATUS_UNENROLLED, service_.GetStatus());
  EXPECT_EQ("StatusUnenrolled", service_.GetStatusString());

  SetStageInLocalState(ConsumerManagementStage::EnrollmentRequested());

  EXPECT_EQ(ConsumerManagementService::STATUS_ENROLLING, service_.GetStatus());
  EXPECT_EQ("StatusEnrolling", service_.GetStatusString());

  SetManagementMode(em::PolicyData::CONSUMER_MANAGED);
  SetStageInLocalState(ConsumerManagementStage::EnrollmentSuccess());

  EXPECT_EQ(ConsumerManagementService::STATUS_ENROLLED, service_.GetStatus());
  EXPECT_EQ("StatusEnrolled", service_.GetStatusString());

  SetStageInLocalState(ConsumerManagementStage::UnenrollmentRequested());
  EXPECT_EQ(ConsumerManagementService::STATUS_UNENROLLING,
            service_.GetStatus());
  EXPECT_EQ("StatusUnenrolling", service_.GetStatusString());

  SetManagementMode(em::PolicyData::LOCAL_OWNER);
  SetStageInLocalState(ConsumerManagementStage::UnenrollmentSuccess());
  EXPECT_EQ(ConsumerManagementService::STATUS_UNENROLLED, service_.GetStatus());
  EXPECT_EQ("StatusUnenrolled", service_.GetStatusString());
}

}  // namespace policy
