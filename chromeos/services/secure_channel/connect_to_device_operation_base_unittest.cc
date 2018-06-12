// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/connect_to_device_operation_base.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chromeos/services/secure_channel/device_id_pair.h"
#include "chromeos/services/secure_channel/public/cpp/shared/fake_authenticated_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

namespace {

const char kTestFailureReason[] = "testFailureReason";
const char kTestRemoteDeviceId[] = "testRemoteDeviceId";
const char kTestLocalDeviceId[] = "testLocalDeviceId";

// Since ConnectToDeviceOperationBase is templatized, a concrete implementation
// is needed for its test.
class TestConnectToDeviceOperation
    : public ConnectToDeviceOperationBase<std::string> {
 public:
  static std::unique_ptr<TestConnectToDeviceOperation> Create(
      ConnectToDeviceOperation<std::string>::ConnectionSuccessCallback
          success_callback,
      ConnectToDeviceOperation<std::string>::ConnectionFailedCallback
          failure_callback,
      const DeviceIdPair& device_id_pair,
      base::OnceClosure destructor_callback) {
    auto test_task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    auto operation = base::WrapUnique(new TestConnectToDeviceOperation(
        std::move(success_callback), std::move(failure_callback),
        device_id_pair, std::move(destructor_callback), test_task_runner));
    test_task_runner->RunUntilIdle();
    return operation;
  }

  ~TestConnectToDeviceOperation() override = default;

  bool has_attempted_connection() { return has_attempted_connection_; }
  bool has_canceled_connection() { return has_canceled_connection_; }

  // ConnectToDeviceOperationBase<std::string>:
  void AttemptConnectionToDevice() override {
    has_attempted_connection_ = true;
  }

  void CancelConnectionAttemptToDevice() override {
    has_canceled_connection_ = true;
  }

  // Make On{Successful|Failed}ConnectionAttempt() public for testing.
  using ConnectToDeviceOperation<std::string>::OnSuccessfulConnectionAttempt;
  using ConnectToDeviceOperation<std::string>::OnFailedConnectionAttempt;

 private:
  TestConnectToDeviceOperation(
      ConnectToDeviceOperation<std::string>::ConnectionSuccessCallback
          success_callback,
      ConnectToDeviceOperation<std::string>::ConnectionFailedCallback
          failure_callback,
      const DeviceIdPair& device_id_pair,
      base::OnceClosure destructor_callback,
      scoped_refptr<base::TestSimpleTaskRunner> test_task_runner)
      : ConnectToDeviceOperationBase<std::string>(
            std::move(success_callback),
            std::move(failure_callback),
            device_id_pair,
            std::move(destructor_callback),
            test_task_runner) {}

  bool has_attempted_connection_ = false;
  bool has_canceled_connection_ = false;
};

}  // namespace

class SecureChannelConnectToDeviceOperationBaseTest : public testing::Test {
 protected:
  SecureChannelConnectToDeviceOperationBaseTest()
      : test_device_id_pair_(kTestRemoteDeviceId, kTestLocalDeviceId) {}

  ~SecureChannelConnectToDeviceOperationBaseTest() override = default;

  void SetUp() override {
    test_operation_ = TestConnectToDeviceOperation::Create(
        base::BindOnce(&SecureChannelConnectToDeviceOperationBaseTest::
                           OnSuccessfulConnectionAttempt,
                       base::Unretained(this)),
        base::BindOnce(&SecureChannelConnectToDeviceOperationBaseTest::
                           OnFailedConnectionAttempt,
                       base::Unretained(this)),
        test_device_id_pair_,
        base::BindOnce(
            &SecureChannelConnectToDeviceOperationBaseTest::OnOperationDeleted,
            base::Unretained(this)));
    EXPECT_TRUE(test_operation_->has_attempted_connection());
  }

  void TearDown() override {
    EXPECT_FALSE(destructor_callback_called_);
    test_operation_.reset();
    EXPECT_TRUE(destructor_callback_called_);
  }

  TestConnectToDeviceOperation* test_operation() {
    return test_operation_.get();
  }

  const AuthenticatedChannel* last_authenticated_channel() const {
    return last_authenticated_channel_.get();
  }

  const std::string& last_failure_detail() const {
    return last_failure_detail_;
  }

 private:
  void OnSuccessfulConnectionAttempt(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel) {
    last_authenticated_channel_ = std::move(authenticated_channel);
  }

  void OnFailedConnectionAttempt(std::string failure_detail) {
    last_failure_detail_ = failure_detail;
  }

  void OnOperationDeleted() { destructor_callback_called_ = true; }

  const base::test::ScopedTaskEnvironment scoped_task_environment_;
  const DeviceIdPair test_device_id_pair_;

  std::unique_ptr<AuthenticatedChannel> last_authenticated_channel_;
  std::string last_failure_detail_;
  bool destructor_callback_called_ = false;

  std::unique_ptr<TestConnectToDeviceOperation> test_operation_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelConnectToDeviceOperationBaseTest);
};

TEST_F(SecureChannelConnectToDeviceOperationBaseTest, Success) {
  auto fake_authenticated_channel =
      std::make_unique<FakeAuthenticatedChannel>();
  auto* fake_authenticated_channel_raw = fake_authenticated_channel.get();
  test_operation()->OnSuccessfulConnectionAttempt(
      std::move(fake_authenticated_channel));
  EXPECT_EQ(fake_authenticated_channel_raw, last_authenticated_channel());
}

TEST_F(SecureChannelConnectToDeviceOperationBaseTest, Failure) {
  test_operation()->OnFailedConnectionAttempt(kTestFailureReason);
  EXPECT_EQ(kTestFailureReason, last_failure_detail());
}

TEST_F(SecureChannelConnectToDeviceOperationBaseTest, Cancelation) {
  test_operation()->Cancel();
  EXPECT_TRUE(test_operation()->has_canceled_connection());
}

}  // namespace secure_channel

}  // namespace chromeos
