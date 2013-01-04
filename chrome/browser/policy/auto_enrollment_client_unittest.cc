// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/auto_enrollment_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/mock_device_management_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

const char* kSerial = "serial";
const char* kSerialHash =
    "\x01\x44\xb1\xde\xfc\xf7\x56\x10\x87\x01\x5f\x8d\x83\x0d\x65\xb1"
    "\x6f\x02\x4a\xd7\xeb\x92\x45\xfc\xd4\xe4\x37\xa1\x55\x2b\x13\x8a";

using ::testing::InSequence;
using ::testing::SaveArg;
using ::testing::_;

class AutoEnrollmentClientTest : public testing::Test {
 protected:
  AutoEnrollmentClientTest()
      : scoped_testing_local_state_(
            TestingBrowserProcess::GetGlobal()),
        local_state_(scoped_testing_local_state_.Get()),
        service_(NULL),
        completion_callback_count_(0) {}

  virtual void SetUp() OVERRIDE {
    CreateClient(kSerial, 4, 8);
    ASSERT_FALSE(local_state_->GetUserPref(prefs::kShouldAutoEnroll));
    ASSERT_FALSE(local_state_->GetUserPref(prefs::kAutoEnrollmentPowerLimit));
  }

  void CreateClient(const std::string& serial,
                    int power_initial,
                    int power_limit) {
    service_ = new MockDeviceManagementService();
    EXPECT_CALL(*service_, StartJob(_, _, _, _, _, _, _))
        .WillRepeatedly(SaveArg<6>(&last_request_));
    base::Closure callback =
        base::Bind(&AutoEnrollmentClientTest::CompletionCallback,
                   base::Unretained(this));
    client_.reset(new AutoEnrollmentClient(callback,
                                           service_,
                                           local_state_,
                                           serial,
                                           power_initial,
                                           power_limit));
  }

  void CompletionCallback() {
    completion_callback_count_++;
  }

  void ServerWillFail(DeviceManagementStatus error) {
    em::DeviceManagementResponse dummy_response;
    EXPECT_CALL(*service_,
                CreateJob(DeviceManagementRequestJob::TYPE_AUTO_ENROLLMENT))
        .WillOnce(service_->FailJob(error));
  }

  void ServerWillReply(int64 modulus, bool with_hashes, bool with_serial_hash) {
    em::DeviceManagementResponse response;
    em::DeviceAutoEnrollmentResponse* enrollment_response =
        response.mutable_auto_enrollment_response();
    if (modulus >= 0)
      enrollment_response->set_expected_modulus(modulus);
    if (with_hashes) {
      for (size_t i = 0; i < 10; ++i) {
        std::string serial = "serial X";
        serial[7] = '0' + i;
        std::string hash = crypto::SHA256HashString(serial);
        enrollment_response->mutable_hash()->Add()->assign(hash);
      }
    }
    if (with_serial_hash) {
      enrollment_response->mutable_hash()->Add()->assign(kSerialHash,
                                               crypto::kSHA256Length);
    }
    EXPECT_CALL(*service_,
                CreateJob(DeviceManagementRequestJob::TYPE_AUTO_ENROLLMENT))
        .WillOnce(service_->SucceedJob(response));
  }

  void VerifyCachedResult(bool should_enroll, int power_limit) {
    base::FundamentalValue value_should_enroll(should_enroll);
    base::FundamentalValue value_power_limit(power_limit);
    EXPECT_TRUE(Value::Equals(
        &value_should_enroll,
        local_state_->GetUserPref(prefs::kShouldAutoEnroll)));
    EXPECT_TRUE(Value::Equals(
        &value_power_limit,
        local_state_->GetUserPref(prefs::kAutoEnrollmentPowerLimit)));
  }

  const em::DeviceAutoEnrollmentRequest& auto_enrollment_request() {
    return last_request_.auto_enrollment_request();
  }

  ScopedTestingLocalState scoped_testing_local_state_;
  TestingPrefServiceSimple* local_state_;
  MockDeviceManagementService* service_;
  scoped_ptr<AutoEnrollmentClient> client_;
  em::DeviceManagementRequest last_request_;
  int completion_callback_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentClientTest);
};

TEST_F(AutoEnrollmentClientTest, NetworkFailure) {
  ServerWillFail(DM_STATUS_TEMPORARY_UNAVAILABLE);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  VerifyCachedResult(false, 8);
}

TEST_F(AutoEnrollmentClientTest, EmptyReply) {
  ServerWillReply(-1, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  VerifyCachedResult(false, 8);
}

TEST_F(AutoEnrollmentClientTest, ClientUploadsRightBits) {
  ServerWillReply(-1, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);

  EXPECT_TRUE(auto_enrollment_request().has_remainder());
  EXPECT_TRUE(auto_enrollment_request().has_modulus());
  EXPECT_EQ(16, auto_enrollment_request().modulus());
  EXPECT_EQ(kSerialHash[31] & 0xf, auto_enrollment_request().remainder());
  VerifyCachedResult(false, 8);
}

TEST_F(AutoEnrollmentClientTest, AskForMoreThenFail) {
  InSequence sequence;
  ServerWillReply(32, false, false);
  ServerWillFail(DM_STATUS_TEMPORARY_UNAVAILABLE);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  VerifyCachedResult(false, 8);
}

TEST_F(AutoEnrollmentClientTest, AskForMoreThenEvenMore) {
  InSequence sequence;
  ServerWillReply(32, false, false);
  ServerWillReply(64, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  VerifyCachedResult(false, 8);
}

TEST_F(AutoEnrollmentClientTest, AskForLess) {
  InSequence sequence;
  ServerWillReply(8, false, false);
  ServerWillReply(-1, true, true);
  client_->Start();
  EXPECT_TRUE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  VerifyCachedResult(true, 8);
}

TEST_F(AutoEnrollmentClientTest, AskForSame) {
  InSequence sequence;
  ServerWillReply(16, false, false);
  ServerWillReply(-1, true, true);
  client_->Start();
  EXPECT_TRUE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  VerifyCachedResult(true, 8);
}

TEST_F(AutoEnrollmentClientTest, AskForSameTwice) {
  InSequence sequence;
  ServerWillReply(16, false, false);
  ServerWillReply(16, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  VerifyCachedResult(false, 8);
}

TEST_F(AutoEnrollmentClientTest, AskForTooMuch) {
  ServerWillReply(512, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  VerifyCachedResult(false, 8);
}

TEST_F(AutoEnrollmentClientTest, AskNonPowerOf2) {
  InSequence sequence;
  ServerWillReply(100, false, false);
  ServerWillReply(-1, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  EXPECT_TRUE(auto_enrollment_request().has_remainder());
  EXPECT_TRUE(auto_enrollment_request().has_modulus());
  EXPECT_EQ(128, auto_enrollment_request().modulus());
  EXPECT_EQ(kSerialHash[31] & 0x7f, auto_enrollment_request().remainder());
  VerifyCachedResult(false, 8);
}

TEST_F(AutoEnrollmentClientTest, ConsumerDevice) {
  ServerWillReply(-1, true, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  VerifyCachedResult(false, 8);
}

TEST_F(AutoEnrollmentClientTest, EnterpriseDevice) {
  ServerWillReply(-1, true, true);
  client_->Start();
  EXPECT_TRUE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  VerifyCachedResult(true, 8);
}

TEST_F(AutoEnrollmentClientTest, NoSerial) {
  CreateClient("", 4, 8);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  EXPECT_FALSE(local_state_->GetUserPref(prefs::kShouldAutoEnroll));
  EXPECT_FALSE(local_state_->GetUserPref(prefs::kAutoEnrollmentPowerLimit));
}

TEST_F(AutoEnrollmentClientTest, NoBitsUploaded) {
  CreateClient(kSerial, 0, 0);
  ServerWillReply(-1, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  EXPECT_TRUE(auto_enrollment_request().has_remainder());
  EXPECT_TRUE(auto_enrollment_request().has_modulus());
  EXPECT_EQ(1, auto_enrollment_request().modulus());
  EXPECT_EQ(0, auto_enrollment_request().remainder());
  VerifyCachedResult(false, 0);
}

TEST_F(AutoEnrollmentClientTest, ManyBitsUploaded) {
  int64 bottom62 = GG_INT64_C(0x14e437a1552b138a);
  for (int i = 0; i <= 62; ++i) {
    completion_callback_count_ = 0;
    CreateClient(kSerial, i, i);
    ServerWillReply(-1, false, false);
    client_->Start();
    EXPECT_FALSE(client_->should_auto_enroll());
    EXPECT_EQ(1, completion_callback_count_);
    EXPECT_TRUE(auto_enrollment_request().has_remainder());
    EXPECT_TRUE(auto_enrollment_request().has_modulus());
    EXPECT_EQ(GG_INT64_C(1) << i, auto_enrollment_request().modulus());
    EXPECT_EQ(bottom62 % (GG_INT64_C(1) << i),
              auto_enrollment_request().remainder());
    VerifyCachedResult(false, i);
  }
}

TEST_F(AutoEnrollmentClientTest, MoreThan32BitsUploaded) {
  CreateClient(kSerial, 10, 37);
  InSequence sequence;
  ServerWillReply(GG_INT64_C(1) << 37, false, false);
  ServerWillReply(-1, true, true);
  client_->Start();
  EXPECT_TRUE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  VerifyCachedResult(true, 37);
}

TEST_F(AutoEnrollmentClientTest, ReuseCachedDecision) {
  EXPECT_CALL(*service_, CreateJob(_)).Times(0);
  local_state_->SetUserPref(prefs::kShouldAutoEnroll,
                            Value::CreateBooleanValue(true));
  local_state_->SetUserPref(prefs::kAutoEnrollmentPowerLimit,
                            Value::CreateIntegerValue(8));
  client_->Start();
  EXPECT_TRUE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  AutoEnrollmentClient::CancelAutoEnrollment();
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(2, completion_callback_count_);
}

TEST_F(AutoEnrollmentClientTest, RetryIfPowerLargerThanCached) {
  local_state_->SetUserPref(prefs::kShouldAutoEnroll,
                            Value::CreateBooleanValue(false));
  local_state_->SetUserPref(prefs::kAutoEnrollmentPowerLimit,
                            Value::CreateIntegerValue(8));
  CreateClient(kSerial, 5, 10);
  ServerWillReply(-1, true, true);
  client_->Start();
  EXPECT_TRUE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
}

}  // namespace

}  // namespace policy
