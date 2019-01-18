// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/android_management_client.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::SaveArg;
using testing::StrictMock;
using testing::_;

namespace em = enterprise_management;

namespace policy {

namespace {

const char kAccountId[] = "fake-account-id";
const char kOAuthToken[] = "fake-oauth-token";

MATCHER_P(MatchProto, expected, "matches protobuf") {
  return arg.SerializePartialAsString() == expected.SerializePartialAsString();
}

}  // namespace

class AndroidManagementClientTest : public testing::Test {
 protected:
  AndroidManagementClientTest()
      : identity_test_environment_(&url_loader_factory_) {
    android_management_request_.mutable_check_android_management_request();
    android_management_response_.mutable_check_android_management_response();
  }

  // testing::Test:
  void SetUp() override {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);
    client_.reset(new AndroidManagementClient(
        &service_, shared_url_loader_factory_, kAccountId,
        identity_test_environment_.identity_manager()));
  }

  // Request protobuf is used as extectation for the client requests.
  em::DeviceManagementRequest android_management_request_;

  // Protobuf is used in successfil responsees.
  em::DeviceManagementResponse android_management_response_;

  base::MessageLoop loop_;
  MockDeviceManagementService service_;
  StrictMock<base::MockCallback<AndroidManagementClient::StatusCallback>>
      callback_observer_;
  std::unique_ptr<AndroidManagementClient> client_;
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  identity::IdentityTestEnvironment identity_test_environment_;
};

TEST_F(AndroidManagementClientTest, CheckAndroidManagementCall) {
  std::string client_id;
  EXPECT_CALL(
      service_,
      CreateJob(DeviceManagementRequestJob::TYPE_ANDROID_MANAGEMENT_CHECK,
                shared_url_loader_factory_))
      .WillOnce(service_.SucceedJob(android_management_response_));
  EXPECT_CALL(service_,
              StartJob(dm_protocol::kValueRequestCheckAndroidManagement,
                       std::string(), kOAuthToken, std::string(), std::string(),
                       _, MatchProto(android_management_request_)))
      .WillOnce(SaveArg<5>(&client_id));
  EXPECT_CALL(callback_observer_,
              Run(AndroidManagementClient::Result::UNMANAGED))
      .Times(1);

  // On ChromeOS platform, account_id and email are same.
  AccountInfo account_info =
      identity_test_environment_.MakeAccountAvailable(kAccountId);

  client_->StartCheckAndroidManagement(callback_observer_.Get());

  identity_test_environment_
      .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
          account_info.account_id, kOAuthToken, base::Time::Max());

  ASSERT_LT(client_id.size(), 64U);
}

}  // namespace policy
