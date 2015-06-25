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
#include "device/bluetooth/bluetooth_low_energy_device_mac.h"
#include "device/bluetooth/bluetooth_low_energy_discovery_manager_mac.h"

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
      public BluetoothDiscoveryManagerMac::Observer,
      public BluetoothLowEnergyDiscoveryManagerMac::Observer {
 public:
  static base::WeakPtr<BluetoothAdapter> CreateAdapter();

  // BluetoothAdapter overrides:
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
  void RegisterAdvertisement(
      scoped_ptr<BluetoothAdvertisement::Data> advertisement_data,
      const CreateAdvertisementCallback& callback,
      const CreateAdvertisementErrorCallback& error_callback) override;

  // BluetoothDiscoveryManagerMac::Observer overrides:
  void ClassicDeviceFound(IOBluetoothDevice* device) override;
  void ClassicDiscoveryStopped(bool unexpected) override;

  // BluetoothLowEnergyDiscoveryManagerMac::Observer override:
  void LowEnergyDeviceUpdated(CBPeripheral* peripheral,
                              NSDictionary* advertisementData,
                              int rssi) override;

  // Registers that a new |device| has connected to the local host.
  void DeviceConnected(IOBluetoothDevice* device);

 protected:
  // BluetoothAdapter override:
  void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

 private:
  friend class BluetoothAdapterMacTest;

  BluetoothAdapterMac();
  ~BluetoothAdapterMac() override;

  // BluetoothAdapter overrides:
  void AddDiscoverySession(BluetoothDiscoveryFilter* discovery_filter,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) override;
  void RemoveDiscoverySession(BluetoothDiscoveryFilter* discovery_filter,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback) override;
  void SetDiscoveryFilter(scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) override;

  // Start classic and/or low energy discovery sessions, according to the
  // filter.  If a discovery session is already running the filter is updated.
  bool StartDiscovery(BluetoothDiscoveryFilter* discovery_filter);

  void Init();
  void InitForTest(scoped_refptr<base::SequencedTaskRunner> ui_task_runner);
  void PollAdapter();

  // Registers that a new |device| has replied to an Inquiry, is paired, or has
  // connected to the local host.
  void ClassicDeviceAdded(IOBluetoothDevice* device);

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

  // Discovery manager for Bluetooth Low Energy.
  scoped_ptr<BluetoothLowEnergyDiscoveryManagerMac>
      low_energy_discovery_manager_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  base::WeakPtrFactory<BluetoothAdapterMac> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_
