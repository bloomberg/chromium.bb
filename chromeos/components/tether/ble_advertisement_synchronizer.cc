// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_advertisement_synchronizer.h"

#include "base/memory/ptr_util.h"
#include "base/time/default_clock.h"

namespace chromeos {

namespace tether {

namespace {

int64_t kTimeBetweenEachCommandMs = 200;

}  // namespace

BleAdvertisementSynchronizer::RegisterArgs::RegisterArgs(
    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
    const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
    const device::BluetoothAdapter::AdvertisementErrorCallback& error_callback)
    : advertisement_data(std::move(advertisement_data)),
      callback(callback),
      error_callback(error_callback) {}

BleAdvertisementSynchronizer::RegisterArgs::~RegisterArgs() {}

BleAdvertisementSynchronizer::UnregisterArgs::UnregisterArgs(
    scoped_refptr<device::BluetoothAdvertisement> advertisement,
    const device::BluetoothAdvertisement::SuccessCallback& callback,
    const device::BluetoothAdvertisement::ErrorCallback& error_callback)
    : advertisement(std::move(advertisement)),
      callback(callback),
      error_callback(error_callback) {}

BleAdvertisementSynchronizer::UnregisterArgs::~UnregisterArgs() {}

BleAdvertisementSynchronizer::Command::Command(
    std::unique_ptr<RegisterArgs> register_args)
    : command_type(CommandType::REGISTER_ADVERTISEMENT),
      register_args(std::move(register_args)) {}

BleAdvertisementSynchronizer::Command::Command(
    std::unique_ptr<UnregisterArgs> unregister_args)
    : command_type(CommandType::UNREGISTER_ADVERTISEMENT),
      unregister_args(std::move(unregister_args)) {}

BleAdvertisementSynchronizer::Command::~Command() {}

BleAdvertisementSynchronizer::BleAdvertisementSynchronizer(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : bluetooth_adapter_(bluetooth_adapter),
      timer_(base::MakeUnique<base::OneShotTimer>()),
      clock_(base::MakeUnique<base::DefaultClock>()),
      weak_ptr_factory_(this) {}

BleAdvertisementSynchronizer::~BleAdvertisementSynchronizer() {}

void BleAdvertisementSynchronizer::RegisterAdvertisement(
    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
    const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
    const device::BluetoothAdapter::AdvertisementErrorCallback&
        error_callback) {
  command_queue_.emplace_back(base::MakeUnique<RegisterArgs>(
      std::move(advertisement_data), callback, error_callback));
  ProcessQueue();
}

void BleAdvertisementSynchronizer::UnregisterAdvertisement(
    scoped_refptr<device::BluetoothAdvertisement> advertisement,
    const device::BluetoothAdvertisement::SuccessCallback& callback,
    const device::BluetoothAdvertisement::ErrorCallback& error_callback) {
  command_queue_.emplace_back(base::MakeUnique<UnregisterArgs>(
      std::move(advertisement), callback, error_callback));
  ProcessQueue();
}

void BleAdvertisementSynchronizer::ProcessQueue() {
  if (command_queue_.empty())
    return;

  // If the timer is already running, there is nothing to do until the timer
  // fires.
  if (timer_->IsRunning())
    return;

  base::Time current_timestamp = clock_->Now();
  base::TimeDelta time_since_last_command =
      current_timestamp - last_command_timestamp_;

  // If we have issued a Bluetooth command in the last kTimeBetweenEachCommandMs
  // milliseconds, wait. Sending commands too frequently can cause race
  // conditions. See crbug.com/760792.
  if (!last_command_timestamp_.is_null() &&
      time_since_last_command <
          base::TimeDelta::FromMilliseconds(kTimeBetweenEachCommandMs)) {
    timer_->Start(FROM_HERE,
                  base::TimeDelta::FromMilliseconds(kTimeBetweenEachCommandMs),
                  base::Bind(&BleAdvertisementSynchronizer::ProcessQueue,
                             weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  last_command_timestamp_ = current_timestamp;

  if (command_queue_.front().command_type ==
      CommandType::REGISTER_ADVERTISEMENT) {
    RegisterArgs* register_args = command_queue_.front().register_args.get();
    DCHECK(register_args);

    bluetooth_adapter_->RegisterAdvertisement(
        std::move(register_args->advertisement_data), register_args->callback,
        register_args->error_callback);
  } else {
    DCHECK(command_queue_.front().command_type ==
           CommandType::UNREGISTER_ADVERTISEMENT);

    UnregisterArgs* unregister_args =
        command_queue_.front().unregister_args.get();
    DCHECK(unregister_args);

    unregister_args->advertisement->Unregister(unregister_args->callback,
                                               unregister_args->error_callback);
  }

  command_queue_.pop_front();

  ProcessQueue();
}

void BleAdvertisementSynchronizer::SetTestDoubles(
    std::unique_ptr<base::Timer> test_timer,
    std::unique_ptr<base::Clock> test_clock) {
  timer_ = std::move(test_timer);
  clock_ = std::move(test_clock);
}

}  // namespace tether

}  // namespace chromeos
