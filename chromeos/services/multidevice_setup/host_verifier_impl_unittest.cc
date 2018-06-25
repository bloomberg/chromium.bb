// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/host_verifier_impl.h"

#include <memory>

#include "base/macros.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/fake_host_backend_delegate.h"
#include "chromeos/services/multidevice_setup/fake_host_verifier.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace multidevice_setup {

class MultiDeviceSetupHostVerifierImplTest : public testing::Test {
 protected:
  MultiDeviceSetupHostVerifierImplTest() = default;
  ~MultiDeviceSetupHostVerifierImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_host_backend_delegate_ = std::make_unique<FakeHostBackendDelegate>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();

    host_verifier_ = HostVerifierImpl::Factory::Get()->BuildInstance(
        fake_host_backend_delegate_.get(), fake_device_sync_client_.get(),
        fake_secure_channel_client_.get());

    fake_observer_ = std::make_unique<FakeHostVerifierObserver>();
    host_verifier_->AddObserver(fake_observer_.get());
  }

  void TearDown() override {
    host_verifier_->RemoveObserver(fake_observer_.get());
  }

 private:
  std::unique_ptr<FakeHostVerifierObserver> fake_observer_;
  std::unique_ptr<FakeHostBackendDelegate> fake_host_backend_delegate_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;

  std::unique_ptr<HostVerifier> host_verifier_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupHostVerifierImplTest);
};

TEST_F(MultiDeviceSetupHostVerifierImplTest, Test) {}

}  // namespace multidevice_setup

}  // namespace chromeos
