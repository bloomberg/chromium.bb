// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/device_info/device_info_data_type_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/base/model_type.h"
#include "components/sync/device_info/local_device_info_provider_mock.h"
#include "components/sync/driver/fake_sync_client.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class DeviceInfoDataTypeControllerTest : public testing::Test {
 public:
  DeviceInfoDataTypeControllerTest()
      : load_finished_(false),
        last_type_(UNSPECIFIED),
        weak_ptr_factory_(this) {}
  ~DeviceInfoDataTypeControllerTest() override {}

  void SetUp() override {
    local_device_ = base::MakeUnique<LocalDeviceInfoProviderMock>(
        "cache_guid", "Wayne Gretzky's Hacking Box", "Chromium 10k",
        "Chrome 10k", sync_pb::SyncEnums_DeviceType_TYPE_LINUX, "device_id");

    controller_ = base::MakeUnique<DeviceInfoDataTypeController>(
        base::Closure(), &sync_client_, local_device_.get());

    load_finished_ = false;
    last_type_ = UNSPECIFIED;
    last_error_ = SyncError();
  }

  void TearDown() override {
    controller_ = nullptr;
    local_device_.reset();
  }

  void Start() {
    controller_->LoadModels(
        base::Bind(&DeviceInfoDataTypeControllerTest::OnLoadFinished,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void OnLoadFinished(ModelType type, const SyncError& error) {
    load_finished_ = true;
    last_type_ = type;
    last_error_ = error;
  }

  testing::AssertionResult LoadResult() {
    if (!load_finished_) {
      return testing::AssertionFailure() << "OnLoadFinished wasn't called";
    }

    if (last_error_.IsSet()) {
      return testing::AssertionFailure()
             << "OnLoadFinished was called with a SyncError: "
             << last_error_.ToString();
    }

    if (last_type_ != DEVICE_INFO) {
      return testing::AssertionFailure()
             << "OnLoadFinished was called with a wrong sync type: "
             << last_type_;
    }

    return testing::AssertionSuccess();
  }

 protected:
  std::unique_ptr<DeviceInfoDataTypeController> controller_;
  std::unique_ptr<LocalDeviceInfoProviderMock> local_device_;
  bool load_finished_;

 private:
  base::MessageLoopForUI message_loop_;
  ModelType last_type_;
  SyncError last_error_;
  FakeSyncClient sync_client_;
  base::WeakPtrFactory<DeviceInfoDataTypeControllerTest> weak_ptr_factory_;
};

TEST_F(DeviceInfoDataTypeControllerTest, StartModels) {
  Start();
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller_->state());
  EXPECT_TRUE(LoadResult());
}

TEST_F(DeviceInfoDataTypeControllerTest, StartModelsDelayedByLocalDevice) {
  local_device_->SetInitialized(false);
  Start();
  EXPECT_FALSE(load_finished_);
  EXPECT_EQ(DataTypeController::MODEL_STARTING, controller_->state());

  local_device_->SetInitialized(true);
  EXPECT_EQ(DataTypeController::MODEL_LOADED, controller_->state());
  EXPECT_TRUE(LoadResult());
}

// Tests that DeviceInfoDataTypeControllerTest handles the situation
// when everything stops before the start gets a chance to finish.
TEST_F(DeviceInfoDataTypeControllerTest, DestructionWithDelayedStart) {
  local_device_->SetInitialized(false);
  Start();

  controller_->Stop();
  // Destroy |local_device_| and |controller_| out of order
  // to verify that the controller doesn't crash in the destructor.
  local_device_.reset();
  controller_ = nullptr;
}

}  // namespace

}  // namespace syncer
