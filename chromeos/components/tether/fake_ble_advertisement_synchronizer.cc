// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_ble_advertisement_synchronizer.h"

namespace chromeos {

namespace tether {

FakeBleAdvertisementSynchronizer::FakeBleAdvertisementSynchronizer()
    : BleAdvertisementSynchronizer(nullptr /* bluetooth_adapter */) {}

FakeBleAdvertisementSynchronizer::~FakeBleAdvertisementSynchronizer() {}

size_t FakeBleAdvertisementSynchronizer::GetNumCommands() {
  return command_queue().size();
}

device::BluetoothAdvertisement::Data&
FakeBleAdvertisementSynchronizer::GetAdvertisementData(size_t index) {
  DCHECK(command_queue().size() >= index);
  DCHECK(command_queue()[index].command_type ==
         CommandType::REGISTER_ADVERTISEMENT);
  return *command_queue()[index].register_args->advertisement_data;
}

const device::BluetoothAdapter::CreateAdvertisementCallback&
FakeBleAdvertisementSynchronizer::GetRegisterCallback(size_t index) {
  DCHECK(command_queue().size() >= index);
  DCHECK(command_queue()[index].command_type ==
         CommandType::REGISTER_ADVERTISEMENT);
  return command_queue()[index].register_args->callback;
}

const device::BluetoothAdapter::AdvertisementErrorCallback&
FakeBleAdvertisementSynchronizer::GetRegisterErrorCallback(size_t index) {
  DCHECK(command_queue().size() >= index);
  DCHECK(command_queue()[index].command_type ==
         CommandType::REGISTER_ADVERTISEMENT);
  return command_queue()[index].register_args->error_callback;
}

const device::BluetoothAdvertisement::SuccessCallback&
FakeBleAdvertisementSynchronizer::GetUnregisterCallback(size_t index) {
  DCHECK(command_queue().size() >= index);
  DCHECK(command_queue()[index].command_type ==
         CommandType::UNREGISTER_ADVERTISEMENT);
  return command_queue()[index].unregister_args->callback;
}

const device::BluetoothAdvertisement::ErrorCallback&
FakeBleAdvertisementSynchronizer::GetUnregisterErrorCallback(size_t index) {
  DCHECK(command_queue().size() >= index);
  DCHECK(command_queue()[index].command_type ==
         CommandType::UNREGISTER_ADVERTISEMENT);
  return command_queue()[index].unregister_args->error_callback;
}

// Intentionally left blank. The fake should not actually issue any commands.
void FakeBleAdvertisementSynchronizer::ProcessQueue() {}

}  // namespace tether

}  // namespace chromeos
