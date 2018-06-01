// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/local_device_metadata_manager.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/unguessable_token.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace secure_channel {

namespace {
const size_t kNumTestDevices = 2;
}  // namespace

class SecureChannelLocalDeviceMetadataManagerTest : public testing::Test {
 protected:
  SecureChannelLocalDeviceMetadataManagerTest()
      : test_device_refs_(
            cryptauth::CreateRemoteDeviceRefListForTest(kNumTestDevices)) {}
  ~SecureChannelLocalDeviceMetadataManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    manager_ = std::make_unique<LocalDeviceMetadataManager>();
  }

  cryptauth::RemoteDeviceRef GetDevice(size_t index) {
    EXPECT_TRUE(index < kNumTestDevices);
    return test_device_refs_[index];
  }

  LocalDeviceMetadataManager* manager() { return manager_.get(); }

  const cryptauth::RemoteDeviceRefList& test_device_refs() {
    return test_device_refs_;
  }

 private:
  const cryptauth::RemoteDeviceRefList test_device_refs_;

  std::unique_ptr<LocalDeviceMetadataManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelLocalDeviceMetadataManagerTest);
};

TEST_F(SecureChannelLocalDeviceMetadataManagerTest, DefaultLocalDevice) {
  // No device should be present before one is added.
  EXPECT_FALSE(manager()->GetDefaultLocalDeviceMetadata());

  // Set the default local device.
  manager()->SetDefaultLocalDeviceDataMetadata(test_device_refs()[0]);
  EXPECT_EQ(test_device_refs()[0], *manager()->GetDefaultLocalDeviceMetadata());

  // Set a new device as the default local device.
  manager()->SetDefaultLocalDeviceDataMetadata(test_device_refs()[1]);
  EXPECT_EQ(test_device_refs()[1], *manager()->GetDefaultLocalDeviceMetadata());
}

TEST_F(SecureChannelLocalDeviceMetadataManagerTest, LocalDevicePerRequest) {
  auto request_id_1 = base::UnguessableToken::Create();
  auto request_id_2 = base::UnguessableToken::Create();

  // First request.
  EXPECT_FALSE(manager()->GetLocalDeviceMetadataForRequest(request_id_1));
  manager()->SetLocalDeviceMetadataForRequest(request_id_1,
                                              test_device_refs()[0]);
  EXPECT_EQ(test_device_refs()[0],
            *manager()->GetLocalDeviceMetadataForRequest(request_id_1));

  // Second request.
  EXPECT_FALSE(manager()->GetLocalDeviceMetadataForRequest(request_id_2));
  manager()->SetLocalDeviceMetadataForRequest(request_id_2,
                                              test_device_refs()[1]);
  EXPECT_EQ(test_device_refs()[1],
            *manager()->GetLocalDeviceMetadataForRequest(request_id_2));

  // Setting new metadata for a request which has already been set should cause
  // a crash.
  EXPECT_DCHECK_DEATH(manager()->SetLocalDeviceMetadataForRequest(
      request_id_1, test_device_refs()[1]));
  EXPECT_DCHECK_DEATH(manager()->SetLocalDeviceMetadataForRequest(
      request_id_2, test_device_refs()[0]));
}

}  // namespace secure_channel

}  // namespace chromeos
