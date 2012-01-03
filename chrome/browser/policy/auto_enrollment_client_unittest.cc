// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/auto_enrollment_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/policy/mock_device_management_service.h"
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
using ::testing::Invoke;
using ::testing::_;

class AutoEnrollmentClientTest : public testing::Test {
 protected:
  AutoEnrollmentClientTest()
      : service_(NULL),
        completion_callback_count_(0) {}

  virtual void SetUp() OVERRIDE {
    CreateClient(kSerial, 4, 8);
  }

  void CreateClient(const std::string& serial,
                    int power_initial,
                    int power_limit) {
    service_ = new MockDeviceManagementService();
    EXPECT_CALL(*service_, StartJob(_))
        .WillRepeatedly(Invoke(this,
                               &AutoEnrollmentClientTest::CaptureRequest));
    base::Closure callback =
        base::Bind(&AutoEnrollmentClientTest::CompletionCallback,
                   base::Unretained(this));
    client_.reset(new AutoEnrollmentClient(callback,
                                           service_,
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

  MockDeviceManagementService* service_;
  scoped_ptr<AutoEnrollmentClient> client_;
  em::DeviceAutoEnrollmentRequest last_request_;
  int completion_callback_count_;

 private:
  void CaptureRequest(DeviceManagementRequestJob* job) {
    last_request_ = job->GetRequest()->auto_enrollment_request();
  }

  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentClientTest);
};

TEST_F(AutoEnrollmentClientTest, NetworkFailure) {
  ServerWillFail(DM_STATUS_TEMPORARY_UNAVAILABLE);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
}

TEST_F(AutoEnrollmentClientTest, EmptyReply) {
  ServerWillReply(-1, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
}

TEST_F(AutoEnrollmentClientTest, ClientUploadsRightBits) {
  ServerWillReply(-1, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  EXPECT_TRUE(last_request_.has_remainder());
  EXPECT_TRUE(last_request_.has_modulus());
  EXPECT_EQ(16, last_request_.modulus());
  EXPECT_EQ(kSerialHash[31] & 0xf, last_request_.remainder());
}

TEST_F(AutoEnrollmentClientTest, AskForMoreThenFail) {
  InSequence sequence;
  ServerWillReply(32, false, false);
  ServerWillFail(DM_STATUS_TEMPORARY_UNAVAILABLE);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
}

TEST_F(AutoEnrollmentClientTest, AskForMoreThenEvenMore) {
  InSequence sequence;
  ServerWillReply(32, false, false);
  ServerWillReply(64, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
}

TEST_F(AutoEnrollmentClientTest, AskForLess) {
  ServerWillReply(8, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
}

TEST_F(AutoEnrollmentClientTest, AskForSame) {
  ServerWillReply(16, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
}

TEST_F(AutoEnrollmentClientTest, AskForTooMuch) {
  ServerWillReply(512, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
}

TEST_F(AutoEnrollmentClientTest, AskNonPowerOf2) {
  InSequence sequence;
  ServerWillReply(100, false, false);
  ServerWillReply(-1, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  EXPECT_TRUE(last_request_.has_remainder());
  EXPECT_TRUE(last_request_.has_modulus());
  EXPECT_EQ(128, last_request_.modulus());
  EXPECT_EQ(kSerialHash[31] & 0x7f, last_request_.remainder());
}

TEST_F(AutoEnrollmentClientTest, ConsumerDevice) {
  ServerWillReply(-1, true, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
}

TEST_F(AutoEnrollmentClientTest, EnterpriseDevice) {
  ServerWillReply(-1, true, true);
  client_->Start();
  EXPECT_TRUE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
}

TEST_F(AutoEnrollmentClientTest, NoSerial) {
  CreateClient("", 4, 8);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
}

TEST_F(AutoEnrollmentClientTest, NoBitsUploaded) {
  CreateClient(kSerial, 0, 0);
  ServerWillReply(-1, false, false);
  client_->Start();
  EXPECT_FALSE(client_->should_auto_enroll());
  EXPECT_EQ(1, completion_callback_count_);
  EXPECT_TRUE(last_request_.has_remainder());
  EXPECT_TRUE(last_request_.has_modulus());
  EXPECT_EQ(1, last_request_.modulus());
  EXPECT_EQ(0, last_request_.remainder());
}

TEST_F(AutoEnrollmentClientTest, ManyBitsUploaded) {
  int64 bottom62 = GG_LONGLONG(0x14e437a1552b138a);
  for (int i = 0; i <= 62; ++i) {
    completion_callback_count_ = 0;
    CreateClient(kSerial, i, i);
    ServerWillReply(-1, false, false);
    client_->Start();
    EXPECT_FALSE(client_->should_auto_enroll());
    EXPECT_EQ(1, completion_callback_count_);
    EXPECT_TRUE(last_request_.has_remainder());
    EXPECT_TRUE(last_request_.has_modulus());
    EXPECT_EQ((int64) 1 << i, last_request_.modulus());
    EXPECT_EQ(bottom62 % ((int64) 1 << i), last_request_.remainder());
  }
}

}  // namespace

}  // namespace policy
