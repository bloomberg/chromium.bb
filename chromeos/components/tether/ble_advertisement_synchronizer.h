// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISEMENT_SYNCHRONIZER_H_
#define CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISEMENT_SYNCHRONIZER_H_

#include <deque>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"

namespace chromeos {

namespace tether {

// Ensures that BLE advertisement registration/unregistration commands are not
// sent back-to-back. Because Bluetooth race conditions exist in the kernel,
// this strategy is necessary to work around potential bugs. Essentially, this
// class is a synchronization wrapper around the Bluetooth API.
class BleAdvertisementSynchronizer {
 public:
  BleAdvertisementSynchronizer(
      scoped_refptr<device::BluetoothAdapter> bluetooth_adapter);
  virtual ~BleAdvertisementSynchronizer();

  void RegisterAdvertisement(
      std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
      const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
      const device::BluetoothAdapter::AdvertisementErrorCallback&
          error_callback);

  void UnregisterAdvertisement(
      scoped_refptr<device::BluetoothAdvertisement> advertisement,
      const device::BluetoothAdvertisement::SuccessCallback& success_callback,
      const device::BluetoothAdvertisement::ErrorCallback& error_callback);

 protected:
  enum class CommandType { REGISTER_ADVERTISEMENT, UNREGISTER_ADVERTISEMENT };

  struct RegisterArgs {
    RegisterArgs(
        std::unique_ptr<device::BluetoothAdvertisement::Data>
            advertisement_data,
        const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
        const device::BluetoothAdapter::AdvertisementErrorCallback&
            error_callback);
    virtual ~RegisterArgs();

    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data;
    device::BluetoothAdapter::CreateAdvertisementCallback callback;
    device::BluetoothAdapter::AdvertisementErrorCallback error_callback;
  };

  struct UnregisterArgs {
    UnregisterArgs(
        scoped_refptr<device::BluetoothAdvertisement> advertisement,
        const device::BluetoothAdvertisement::SuccessCallback& callback,
        const device::BluetoothAdvertisement::ErrorCallback& error_callback);
    virtual ~UnregisterArgs();

    scoped_refptr<device::BluetoothAdvertisement> advertisement;
    device::BluetoothAdvertisement::SuccessCallback callback;
    device::BluetoothAdvertisement::ErrorCallback error_callback;
  };

  struct Command {
    explicit Command(std::unique_ptr<RegisterArgs> register_args);
    explicit Command(std::unique_ptr<UnregisterArgs> unregister_args);
    virtual ~Command();

    CommandType command_type;
    std::unique_ptr<RegisterArgs> register_args;
    std::unique_ptr<UnregisterArgs> unregister_args;
  };

  virtual void ProcessQueue();

  const std::deque<Command>& command_queue() { return command_queue_; }

 private:
  friend class BleAdvertisementSynchronizerTest;

  void SetTestDoubles(std::unique_ptr<base::Timer> test_timer,
                      std::unique_ptr<base::Clock> test_clock);

  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  std::deque<Command> command_queue_;
  std::unique_ptr<base::Timer> timer_;
  std::unique_ptr<base::Clock> clock_;
  base::Time last_command_timestamp_;
  base::WeakPtrFactory<BleAdvertisementSynchronizer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BleAdvertisementSynchronizer);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_BLE_ADVERTISEMENT_SYNCHRONIZER_H_
