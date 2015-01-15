// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_

#include <IOKit/IOReturn.h>

#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_audio_sink.h"
#include "device/bluetooth/bluetooth_discovery_manager_mac.h"
#include "device/bluetooth/bluetooth_export.h"

@class IOBluetoothDevice;
@class NSArray;
@class NSDate;

namespace base {

class SequencedTaskRunner;

}  // namespace base

namespace device {

class BluetoothAdapterMacTest;

class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterMac
    : public BluetoothAdapter,
      public BluetoothDiscoveryManagerMac::Observer {
 public:
  static base::WeakPtr<BluetoothAdapter> CreateAdapter();

  // BluetoothAdapter:
  void AddObserver(BluetoothAdapter::Observer* observer) override;
  void RemoveObserver(BluetoothAdapter::Observer* observer) override;
  std::string GetAddress() const override;
  std::string GetName() const override;
  void SetName(const std::string& name,
               const base::Closure& callback,
               const ErrorCallback& error_callback) override;
  bool IsInitialized() const override;
  bool IsPresent() const override;
  bool IsPowered() const override;
  void SetPowered(bool powered,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  bool IsDiscoverable() const override;
  void SetDiscoverable(bool discoverable,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override;
  bool IsDiscovering() const override;
  void CreateRfcommService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void CreateL2capService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void RegisterAudioSink(
      const BluetoothAudioSink::Options& options,
      const AcquiredCallback& callback,
      const BluetoothAudioSink::ErrorCallback& error_callback) override;

  // BluetoothDiscoveryManagerMac::Observer overrides
  void DeviceFound(IOBluetoothDevice* device) override;
  void DiscoveryStopped(bool unexpected) override;

  // Registers that a new |device| has connected to the local host.
  void DeviceConnected(IOBluetoothDevice* device);

 protected:
  // BluetoothAdapter:
  void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

 private:
  friend class base::DeleteHelper<BluetoothAdapterMac>;
  friend class BluetoothAdapterMacTest;

  BluetoothAdapterMac();
  ~BluetoothAdapterMac() override;

  // BluetoothAdapter:
  void DeleteOnCorrectThread() const override;
  void AddDiscoverySession(const base::Closure& callback,
                           const ErrorCallback& error_callback) override;
  void RemoveDiscoverySession(const base::Closure& callback,
                              const ErrorCallback& error_callback) override;

  void Init();
  void InitForTest(scoped_refptr<base::SequencedTaskRunner> ui_task_runner);
  void PollAdapter();

  // Registers that a new |device| has replied to an Inquiry, is paired, or has
  // connected to the local host.
  void DeviceAdded(IOBluetoothDevice* device);

  // Updates |devices_| to include the currently paired devices, as well as any
  // connected, but unpaired, devices. Notifies observers if any previously
  // paired or connected devices are no longer present.
  void UpdateDevices();

  std::string address_;
  std::string name_;
  bool powered_;

  int num_discovery_sessions_;

  // Discovery manager for Bluetooth Classic.
  scoped_ptr<BluetoothDiscoveryManagerMac> classic_discovery_manager_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothAdapter::Observer> observers_;

  base::WeakPtrFactory<BluetoothAdapterMac> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_
