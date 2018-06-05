// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/ble_service_data_helper_impl.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/cryptauth/remote_device_cache.h"

namespace chromeos {

namespace secure_channel {

// static
BleServiceDataHelperImpl::Factory*
    BleServiceDataHelperImpl::Factory::test_factory_ = nullptr;

// static
BleServiceDataHelperImpl::Factory* BleServiceDataHelperImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void BleServiceDataHelperImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

BleServiceDataHelperImpl::Factory::~Factory() = default;

std::unique_ptr<BleServiceDataHelper>
BleServiceDataHelperImpl::Factory::BuildInstance(
    cryptauth::RemoteDeviceCache* remote_device_cache) {
  return base::WrapUnique(new BleServiceDataHelperImpl(remote_device_cache));
}

BleServiceDataHelperImpl::BleServiceDataHelperImpl(
    cryptauth::RemoteDeviceCache* remote_device_cache)
    : remote_device_cache_(remote_device_cache) {}

BleServiceDataHelperImpl::~BleServiceDataHelperImpl() = default;

std::unique_ptr<cryptauth::DataWithTimestamp>
BleServiceDataHelperImpl::GenerateForegroundAdvertisement(
    const DeviceIdPair& device_id_pair) {
  // TODO(hansberry): Implement.
  NOTIMPLEMENTED();

  // Use remote_device_cache_ to prevent compiler warning.
  // TODO(hansberry): Remove.
  remote_device_cache_->GetRemoteDevices();
  return nullptr;
}

base::Optional<BleServiceDataHelper::DeviceWithBackgroundBool>
BleServiceDataHelperImpl::PerformIdentifyRemoteDevice(
    const std::string& service_data,
    const DeviceIdPairSet& device_id_pair_set) {
  // TODO(hansberry): Implement.
  NOTIMPLEMENTED();
  return base::nullopt;
}

}  // namespace secure_channel

}  // namespace chromeos
