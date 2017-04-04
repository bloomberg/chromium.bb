// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/keep_alive_scheduler.h"

#include <memory>
#include <vector>

#include "base/timer/mock_timer.h"
#include "chromeos/components/tether/fake_active_host.h"
#include "chromeos/components/tether/fake_ble_connection_manager.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace tether {

namespace {

const char kTetherNetworkGuid[] = "tetherNetworkGuid";
const char kWifiNetworkGuid[] = "wifiNetworkGuid";

class OperationDeletedHandler {
 public:
  virtual void OnOperationDeleted() = 0;
};

class FakeKeepAliveOperation : public KeepAliveOperation {
 public:
  FakeKeepAliveOperation(const cryptauth::RemoteDevice& device_to_connect,
                         BleConnectionManager* connection_manager,
                         OperationDeletedHandler* handler)
      : KeepAliveOperation(device_to_connect, connection_manager),
        handler_(handler),
        remote_device_(device_to_connect) {}

  ~FakeKeepAliveOperation() override { handler_->OnOperationDeleted(); }

  void SendOperationFinishedEvent() { OnOperationFinished(); }

  cryptauth::RemoteDevice remote_device() { return remote_device_; }

 private:
  OperationDeletedHandler* handler_;
  const cryptauth::RemoteDevice remote_device_;
};

class FakeKeepAliveOperationFactory : public KeepAliveOperation::Factory,
                                      public OperationDeletedHandler {
 public:
  FakeKeepAliveOperationFactory()
      : num_created_(0), num_deleted_(0), last_created_(nullptr) {}
  ~FakeKeepAliveOperationFactory() {}

  uint32_t num_created() { return num_created_; }

  uint32_t num_deleted() { return num_deleted_; }

  FakeKeepAliveOperation* last_created() { return last_created_; }

  void OnOperationDeleted() override { num_deleted_++; }

 protected:
  std::unique_ptr<KeepAliveOperation> BuildInstance(
      const cryptauth::RemoteDevice& device_to_connect,
      BleConnectionManager* connection_manager) override {
    num_created_++;
    last_created_ =
        new FakeKeepAliveOperation(device_to_connect, connection_manager, this);
    return base::WrapUnique(last_created_);
  }

 private:
  uint32_t num_created_;
  uint32_t num_deleted_;
  FakeKeepAliveOperation* last_created_;
};

}  // namespace

class KeepAliveSchedulerTest : public testing::Test {
 protected:
  KeepAliveSchedulerTest()
      : test_devices_(cryptauth::GenerateTestRemoteDevices(2)) {}

  void SetUp() override {
    fake_active_host_ = base::MakeUnique<FakeActiveHost>();
    fake_ble_connection_manager_ = base::MakeUnique<FakeBleConnectionManager>();
    mock_timer_ = new base::MockTimer(true /* retain_user_task */,
                                      true /* is_repeating */);

    fake_operation_factory_ =
        base::WrapUnique(new FakeKeepAliveOperationFactory());
    KeepAliveOperation::Factory::SetInstanceForTesting(
        fake_operation_factory_.get());

    scheduler_ = base::WrapUnique(new KeepAliveScheduler(
        fake_active_host_.get(), fake_ble_connection_manager_.get(),
        base::WrapUnique(mock_timer_)));
  }

  void VerifyTimerRunning(bool is_running) {
    EXPECT_EQ(is_running, mock_timer_->IsRunning());

    if (is_running) {
      EXPECT_EQ(base::TimeDelta::FromMinutes(
                    KeepAliveScheduler::kKeepAliveIntervalMinutes),
                mock_timer_->GetCurrentDelay());
    }
  }

  const std::vector<cryptauth::RemoteDevice> test_devices_;

  std::unique_ptr<FakeActiveHost> fake_active_host_;
  std::unique_ptr<FakeBleConnectionManager> fake_ble_connection_manager_;
  base::MockTimer* mock_timer_;

  std::unique_ptr<FakeKeepAliveOperationFactory> fake_operation_factory_;

  std::unique_ptr<KeepAliveScheduler> scheduler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeepAliveSchedulerTest);
};

TEST_F(KeepAliveSchedulerTest, TestSendTickle_OneActiveHost) {
  EXPECT_FALSE(fake_operation_factory_->num_created());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);

  // Start connecting to a device. No operation should be started.
  fake_active_host_->SetActiveHostConnecting(test_devices_[0].GetDeviceId(),
                                             std::string(kTetherNetworkGuid));
  EXPECT_FALSE(fake_operation_factory_->num_created());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);

  // Connect to the device; the operation should be started.
  fake_active_host_->SetActiveHostConnected(test_devices_[0].GetDeviceId(),
                                            std::string(kTetherNetworkGuid),
                                            std::string(kWifiNetworkGuid));
  EXPECT_EQ(1u, fake_operation_factory_->num_created());
  EXPECT_EQ(test_devices_[0],
            fake_operation_factory_->last_created()->remote_device());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);

  // Ensure that once the operation is finished, it is deleted.
  fake_operation_factory_->last_created()->SendOperationFinishedEvent();
  EXPECT_EQ(1u, fake_operation_factory_->num_created());
  EXPECT_EQ(1u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);

  // Fire the timer; this should result in another tickle being sent.
  mock_timer_->Fire();
  EXPECT_EQ(2u, fake_operation_factory_->num_created());
  EXPECT_EQ(test_devices_[0],
            fake_operation_factory_->last_created()->remote_device());
  EXPECT_EQ(1u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);

  // Finish this operation.
  fake_operation_factory_->last_created()->SendOperationFinishedEvent();
  EXPECT_EQ(2u, fake_operation_factory_->num_created());
  EXPECT_EQ(2u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);

  // Disconnect that device.
  fake_active_host_->SetActiveHostDisconnected();
  EXPECT_EQ(2u, fake_operation_factory_->num_created());
  EXPECT_EQ(2u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);
}

TEST_F(KeepAliveSchedulerTest, TestSendTickle_MultipleActiveHosts) {
  EXPECT_FALSE(fake_operation_factory_->num_created());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);

  // Start connecting to a device. No operation should be started.
  fake_active_host_->SetActiveHostConnecting(test_devices_[0].GetDeviceId(),
                                             std::string(kTetherNetworkGuid));
  EXPECT_FALSE(fake_operation_factory_->num_created());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);

  // Connect to the device; the operation should be started.
  fake_active_host_->SetActiveHostConnected(test_devices_[0].GetDeviceId(),
                                            std::string(kTetherNetworkGuid),
                                            std::string(kWifiNetworkGuid));
  EXPECT_EQ(1u, fake_operation_factory_->num_created());
  EXPECT_EQ(test_devices_[0],
            fake_operation_factory_->last_created()->remote_device());
  EXPECT_FALSE(fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);

  // Disconnect that device before the operation is finished. It should still be
  // deleted.
  fake_active_host_->SetActiveHostDisconnected();
  EXPECT_EQ(1u, fake_operation_factory_->num_created());
  EXPECT_EQ(1u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);

  // Start connecting to a different. No operation should be started.
  fake_active_host_->SetActiveHostConnecting(test_devices_[1].GetDeviceId(),
                                             std::string(kTetherNetworkGuid));
  EXPECT_EQ(1u, fake_operation_factory_->num_created());
  EXPECT_EQ(1u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);

  // Connect to the second device; the operation should be started.
  fake_active_host_->SetActiveHostConnected(test_devices_[1].GetDeviceId(),
                                            std::string(kTetherNetworkGuid),
                                            std::string(kWifiNetworkGuid));
  EXPECT_EQ(2u, fake_operation_factory_->num_created());
  EXPECT_EQ(test_devices_[1],
            fake_operation_factory_->last_created()->remote_device());
  EXPECT_EQ(1u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);

  // Ensure that once the second operation is finished, it is deleted.
  fake_operation_factory_->last_created()->SendOperationFinishedEvent();
  EXPECT_EQ(2u, fake_operation_factory_->num_created());
  EXPECT_EQ(2u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(true /* is_running */);

  // Disconnect that device.
  fake_active_host_->SetActiveHostDisconnected();
  EXPECT_EQ(2u, fake_operation_factory_->num_created());
  EXPECT_EQ(2u, fake_operation_factory_->num_deleted());
  VerifyTimerRunning(false /* is_running */);
}

}  // namespace tether

}  // namespace cryptauth
