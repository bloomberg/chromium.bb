// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/consumer_management_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/mock_cryptohome_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::NiceMock;
using testing::_;

namespace {
const char* kAttributeOwnerId = "consumer_management.owner_id";
const char* kTestOwner = "test@chromium.org.test";
}

namespace policy {

class ConsumerManagementServiceTest : public testing::Test {
 public:
  ConsumerManagementServiceTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()),
        service_(new ConsumerManagementService(&mock_cryptohome_client_)),
        cryptohome_result_(false),
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

  ScopedTestingLocalState testing_local_state_;
  NiceMock<chromeos::MockCryptohomeClient> mock_cryptohome_client_;
  scoped_ptr<ConsumerManagementService> service_;

  chromeos::DBusMethodCallStatus cryptohome_status_;
  bool cryptohome_result_;
  cryptohome::BaseReply cryptohome_reply_;
  cryptohome::GetBootAttributeRequest get_boot_attribute_request_;
  cryptohome::SetBootAttributeRequest set_boot_attribute_request_;

  std::string owner_;
  bool set_owner_status_;
};

TEST_F(ConsumerManagementServiceTest, CanGetEnrollmentState) {
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_NONE,
            service_->GetEnrollmentState());

  testing_local_state_.Get()->SetInteger(
      prefs::kConsumerManagementEnrollmentState,
      ConsumerManagementService::ENROLLMENT_ENROLLING);

  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_ENROLLING,
            service_->GetEnrollmentState());
}

TEST_F(ConsumerManagementServiceTest, CanSetEnrollmentState) {
  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_NONE,
            testing_local_state_.Get()->GetInteger(
                prefs::kConsumerManagementEnrollmentState));

  service_->SetEnrollmentState(ConsumerManagementService::ENROLLMENT_ENROLLING);

  EXPECT_EQ(ConsumerManagementService::ENROLLMENT_ENROLLING,
            testing_local_state_.Get()->GetInteger(
                prefs::kConsumerManagementEnrollmentState));
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

}  // namespace policy
