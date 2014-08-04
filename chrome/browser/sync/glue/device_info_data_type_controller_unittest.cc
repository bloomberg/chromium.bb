// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/sync/glue/device_info_data_type_controller.h"
#include "chrome/browser/sync/glue/local_device_info_provider_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_driver::DataTypeController;

namespace browser_sync {

namespace {

class DeviceInfoDataTypeControllerTest : public testing::Test {
 public:
  DeviceInfoDataTypeControllerTest()
      : load_finished_(false),
        thread_bundle_(content::TestBrowserThreadBundle::DEFAULT),
        weak_ptr_factory_(this),
        last_type_(syncer::UNSPECIFIED) {}
  virtual ~DeviceInfoDataTypeControllerTest() {}

  virtual void SetUp() OVERRIDE {
    local_device_.reset(new LocalDeviceInfoProviderMock(
        "cache_guid",
        "Wayne Gretzky's Hacking Box",
        "Chromium 10k",
        "Chrome 10k",
        sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
        "device_id"));

    controller_ = new DeviceInfoDataTypeController(
        &profile_sync_factory_,
        local_device_.get(),
        DataTypeController::DisableTypeCallback());

    load_finished_ = false;
    last_type_ = syncer::UNSPECIFIED;
    last_error_ = syncer::SyncError();
  }

  virtual void TearDown() OVERRIDE {
    controller_ = NULL;
    local_device_.reset();
  }

  void Start() {
    controller_->LoadModels(
        base::Bind(&DeviceInfoDataTypeControllerTest::OnLoadFinished,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void OnLoadFinished(syncer::ModelType type, syncer::SyncError error) {
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

    if (last_type_ != syncer::DEVICE_INFO) {
      return testing::AssertionFailure()
             << "OnLoadFinished was called with a wrong sync type: "
             << last_type_;
    }

    return testing::AssertionSuccess();
  }

 protected:
  scoped_refptr<DeviceInfoDataTypeController> controller_;
  scoped_ptr<LocalDeviceInfoProviderMock> local_device_;
  bool load_finished_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  ProfileSyncComponentsFactoryMock profile_sync_factory_;
  base::WeakPtrFactory<DeviceInfoDataTypeControllerTest> weak_ptr_factory_;
  syncer::ModelType last_type_;
  syncer::SyncError last_error_;
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

}  // namespace

}  // namespace browser_sync
