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
#include "device/bluetooth/bluetooth_discovery_manager_mac.h"

@class IOBluetoothDevice;
@class NSArray;
@class NSDate;

namespace base {

class SequencedTaskRunner;

}  // namespace base

namespace device {

class BluetoothAdapterMacTest;

class BluetoothAdapterMac : public BluetoothAdapter,
                            public BluetoothDiscoveryManagerMac::Observer {
 public:
  static base::WeakPtr<BluetoothAdapter> CreateAdapter();

  // BluetoothAdapter:
  virtual void AddObserver(BluetoothAdapter::Observer* observer) override;
  virtual void RemoveObserver(BluetoothAdapter::Observer* observer) override;
  virtual std::string GetAddress() const override;
  virtual std::string GetName() const override;
  virtual void SetName(const std::string& name,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override;
  virtual bool IsInitialized() const override;
  virtual bool IsPresent() const override;
  virtual bool IsPowered() const override;
  virtual void SetPowered(
      bool powered,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;
  virtual bool IsDiscoverable() const override;
  virtual void SetDiscoverable(
      bool discoverable,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;
  virtual bool IsDiscovering() const override;
  virtual void CreateRfcommService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  virtual void CreateL2capService(
      const BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;

  // BluetoothDiscoveryManagerMac::Observer overrides
  virtual void DeviceFound(IOBluetoothDevice* device) override;
  virtual void DiscoveryStopped(bool unexpected) override;

  // Registers that a new |device| has connected to the local host.
  void DeviceConnected(IOBluetoothDevice* device);

 protected:
  // BluetoothAdapter:
  virtual void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

 private:
  friend class BluetoothAdapterMacTest;

  BluetoothAdapterMac();
  virtual ~BluetoothAdapterMac();

  // BluetoothAdapter:
  virtual void AddDiscoverySession(
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;
  virtual void RemoveDiscoverySession(
      const base::Closure& callback,
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
