// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/error_tolerant_ble_advertisement_impl.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/tether/ble_constants.h"
#include "components/cryptauth/remote_device.h"
#include "components/proximity_auth/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace chromeos {

namespace tether {

namespace {

const uint8_t kInvertedConnectionFlag = 0x01;
const size_t kTimeBetweenAttemptsMs = 200u;

}  // namespace

// static
ErrorTolerantBleAdvertisementImpl::Factory*
    ErrorTolerantBleAdvertisementImpl::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<ErrorTolerantBleAdvertisement>
ErrorTolerantBleAdvertisementImpl::Factory::NewInstance(
    const std::string& device_id,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    std::unique_ptr<cryptauth::DataWithTimestamp> advertisement_data) {
  if (!factory_instance_)
    factory_instance_ = new Factory();

  return factory_instance_->BuildInstance(device_id, bluetooth_adapter,
                                          std::move(advertisement_data));
}

// static
void ErrorTolerantBleAdvertisementImpl::Factory::SetInstanceForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

std::unique_ptr<ErrorTolerantBleAdvertisement>
ErrorTolerantBleAdvertisementImpl::Factory::BuildInstance(
    const std::string& device_id,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    std::unique_ptr<cryptauth::DataWithTimestamp> advertisement_data) {
  return base::MakeUnique<ErrorTolerantBleAdvertisementImpl>(
      device_id, bluetooth_adapter, std::move(advertisement_data));
}

ErrorTolerantBleAdvertisementImpl::Factory::~Factory() {}

ErrorTolerantBleAdvertisementImpl::ErrorTolerantBleAdvertisementImpl(
    const std::string& device_id,
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter,
    std::unique_ptr<cryptauth::DataWithTimestamp> advertisement_data)
    : ErrorTolerantBleAdvertisement(device_id),
      bluetooth_adapter_(bluetooth_adapter),
      advertisement_data_(std::move(advertisement_data)),
      timer_(base::MakeUnique<base::OneShotTimer>()),
      weak_ptr_factory_(this) {
  UpdateRegistrationStatus();
}

ErrorTolerantBleAdvertisementImpl::~ErrorTolerantBleAdvertisementImpl() {
  if (advertisement_)
    advertisement_->RemoveObserver(this);
}

void ErrorTolerantBleAdvertisementImpl::Stop(const base::Closure& callback) {
  // Stop() should only be called once per instance.
  DCHECK(stop_callback_.is_null());

  stop_callback_ = callback;
  UpdateRegistrationStatus();
}

bool ErrorTolerantBleAdvertisementImpl::HasBeenStopped() {
  return !stop_callback_.is_null();
}

void ErrorTolerantBleAdvertisementImpl::AdvertisementReleased(
    device::BluetoothAdvertisement* advertisement) {
  DCHECK(advertisement_.get() == advertisement);

  // If the advertisement was released, delete it and try again. Note that this
  // situation is not expected to occur under normal circumstances.
  advertisement_->RemoveObserver(this);
  advertisement_ = nullptr;

  PA_LOG(WARNING) << "Advertisement was released. Trying again. Device ID: \""
                  << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(
                         device_id())
                  << "\", Service data: " << advertisement_data_->DataInHex();

  UpdateRegistrationStatus();
}

void ErrorTolerantBleAdvertisementImpl::SetFakeTimerForTest(
    std::unique_ptr<base::Timer> test_timer) {
  timer_ = std::move(test_timer);
}

void ErrorTolerantBleAdvertisementImpl::UpdateRegistrationStatus() {
  if (!advertisement_ && stop_callback_.is_null())
    AttemptRegistration();
  else if (advertisement_ && !stop_callback_.is_null())
    AttemptUnregistration();
}

void ErrorTolerantBleAdvertisementImpl::RetryUpdateAfterTimer() {
  timer_->Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kTimeBetweenAttemptsMs),
      base::Bind(&ErrorTolerantBleAdvertisementImpl::UpdateRegistrationStatus,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ErrorTolerantBleAdvertisementImpl::AttemptRegistration() {
  // Should never attempt to register after Stop() has been called.
  DCHECK(stop_callback_.is_null() && !unregistration_in_progress_);

  if (registration_in_progress_)
    return;

  registration_in_progress_ = true;

  std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data =
      base::MakeUnique<device::BluetoothAdvertisement::Data>(
          device::BluetoothAdvertisement::AdvertisementType::
              ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_service_uuids(CreateServiceUuids());
  advertisement_data->set_service_data(CreateServiceData());

  bluetooth_adapter_->RegisterAdvertisement(
      std::move(advertisement_data),
      base::Bind(&ErrorTolerantBleAdvertisementImpl::OnAdvertisementRegistered,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(
          &ErrorTolerantBleAdvertisementImpl::OnErrorRegisteringAdvertisement,
          weak_ptr_factory_.GetWeakPtr()));
}

void ErrorTolerantBleAdvertisementImpl::AttemptUnregistration() {
  // Should never attempt to unregister before Stop() has been called.
  DCHECK(!stop_callback_.is_null());

  // If no advertisement has yet been registered, we must wait until it has been
  // successfully registered before it is possible to unregister. Likewise, if
  // unregistration is still in progress, there is nothing else to do.
  if (registration_in_progress_ || unregistration_in_progress_)
    return;

  unregistration_in_progress_ = true;

  advertisement_->Unregister(
      base::Bind(
          &ErrorTolerantBleAdvertisementImpl::OnAdvertisementUnregistered,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(
          &ErrorTolerantBleAdvertisementImpl::OnErrorUnregisteringAdvertisement,
          weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<device::BluetoothAdvertisement::UUIDList>
ErrorTolerantBleAdvertisementImpl::CreateServiceUuids() const {
  std::unique_ptr<device::BluetoothAdvertisement::UUIDList> list =
      base::MakeUnique<device::BluetoothAdvertisement::UUIDList>();
  list->push_back(kAdvertisingServiceUuid);
  return list;
}

std::unique_ptr<device::BluetoothAdvertisement::ServiceData>
ErrorTolerantBleAdvertisementImpl::CreateServiceData() const {
  DCHECK(!advertisement_data_->data.empty());

  std::vector<uint8_t> data_as_vector(advertisement_data_->data.size());
  memcpy(data_as_vector.data(), advertisement_data_->data.data(),
         advertisement_data_->data.size());

  // Add a flag at the end of the service data to signify that the inverted
  // connection flow should be used.
  data_as_vector.push_back(kInvertedConnectionFlag);

  std::unique_ptr<device::BluetoothAdvertisement::ServiceData> service_data =
      base::MakeUnique<device::BluetoothAdvertisement::ServiceData>();
  service_data->insert(std::pair<std::string, std::vector<uint8_t>>(
      kAdvertisingServiceUuid, data_as_vector));
  return service_data;
}

void ErrorTolerantBleAdvertisementImpl::OnAdvertisementRegistered(
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  registration_in_progress_ = false;

  advertisement_ = advertisement;
  advertisement_->AddObserver(this);

  PA_LOG(INFO) << "Advertisement registered. Device ID: \""
               << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(device_id())
               << "\", Service data: " << advertisement_data_->DataInHex();

  UpdateRegistrationStatus();
}

void ErrorTolerantBleAdvertisementImpl::OnErrorRegisteringAdvertisement(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  registration_in_progress_ = false;

  PA_LOG(ERROR) << "Error registering advertisement. Device ID: \""
                << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(device_id())
                << "\", Service data: " << advertisement_data_->DataInHex()
                << ", Error code: " << error_code;

  // Try again, but wait kTimeBetweenAttemptsMs to avoid spamming the Bluetooth
  // controller.
  RetryUpdateAfterTimer();
}

void ErrorTolerantBleAdvertisementImpl::OnAdvertisementUnregistered() {
  unregistration_in_progress_ = false;

  advertisement_->RemoveObserver(this);
  advertisement_ = nullptr;

  DCHECK(!stop_callback_.is_null());
  stop_callback_.Run();
}

void ErrorTolerantBleAdvertisementImpl::OnErrorUnregisteringAdvertisement(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  unregistration_in_progress_ = false;

  PA_LOG(ERROR) << "Error unregistering advertisement. Device ID: \""
                << cryptauth::RemoteDevice::TruncateDeviceIdForLogs(device_id())
                << "\", Service data: " << advertisement_data_->DataInHex()
                << ", Error code: " << error_code;

  // Try again, but wait kTimeBetweenAttemptsMs to avoid spamming the Bluetooth
  // controller.
  RetryUpdateAfterTimer();
}

}  // namespace tether

}  // namespace chromeos
