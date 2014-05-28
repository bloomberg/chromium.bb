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

@class BluetoothAdapterMacDelegate;
@class IOBluetoothDevice;
@class IOBluetoothDeviceInquiry;
@class NSArray;
@class NSDate;

namespace base {

class SequencedTaskRunner;

}  // namespace base

namespace device {

class BluetoothAdapterMacTest;

class BluetoothAdapterMac : public BluetoothAdapter {
 public:
  static base::WeakPtr<BluetoothAdapter> CreateAdapter();

  // BluetoothAdapter:
  virtual void AddObserver(BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual std::string GetAddress() const OVERRIDE;
  virtual std::string GetName() const OVERRIDE;
  virtual void SetName(const std::string& name,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsInitialized() const OVERRIDE;
  virtual bool IsPresent() const OVERRIDE;
  virtual bool IsPowered() const OVERRIDE;
  virtual void SetPowered(
      bool powered,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsDiscoverable() const OVERRIDE;
  virtual void SetDiscoverable(
      bool discoverable,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsDiscovering() const OVERRIDE;
  virtual void CreateRfcommService(
      const BluetoothUUID& uuid,
      int channel,
      bool insecure,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) OVERRIDE;
  virtual void CreateL2capService(
      const BluetoothUUID& uuid,
      int psm,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) OVERRIDE;

  // called by BluetoothAdapterMacDelegate.
  void DeviceInquiryStarted(IOBluetoothDeviceInquiry* inquiry);
  void DeviceFound(IOBluetoothDeviceInquiry* inquiry,
                   IOBluetoothDevice* device);
  void DeviceInquiryComplete(IOBluetoothDeviceInquiry* inquiry,
                             IOReturn error,
                             bool aborted);

 protected:
  // BluetoothAdapter:
  virtual void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) OVERRIDE;

 private:
  friend class BluetoothAdapterMacTest;

  enum DiscoveryStatus {
    NOT_DISCOVERING,
    DISCOVERY_STARTING,
    DISCOVERING,
    DISCOVERY_STOPPING
  };

  BluetoothAdapterMac();
  virtual ~BluetoothAdapterMac();

  // BluetoothAdapter:
  virtual void AddDiscoverySession(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void RemoveDiscoverySession(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;

  void Init();
  void InitForTest(scoped_refptr<base::SequencedTaskRunner> ui_task_runner);
  void PollAdapter();

  // Updates |devices_| to be consistent with |devices|.
  void UpdateDevices(NSArray* devices);

  void MaybeStartDeviceInquiry();
  void MaybeStopDeviceInquiry();

  typedef std::vector<std::pair<base::Closure, ErrorCallback> >
      DiscoveryCallbackList;
  void RunCallbacks(const DiscoveryCallbackList& callback_list,
                    bool success) const;

  std::string address_;
  std::string name_;
  bool powered_;
  DiscoveryStatus discovery_status_;

  DiscoveryCallbackList on_start_discovery_callbacks_;
  DiscoveryCallbackList on_stop_discovery_callbacks_;
  size_t num_discovery_listeners_;

  base::scoped_nsobject<BluetoothAdapterMacDelegate> adapter_delegate_;
  base::scoped_nsobject<IOBluetoothDeviceInquiry> device_inquiry_;

  // A list of discovered device addresses.
  // This list is used to check if the same device is discovered twice during
  // the discovery between consecutive inquiries.
  base::hash_set<std::string> discovered_devices_;

  // Timestamp for the recently accessed device.
  // Used to determine if |devices_| needs an update.
  base::scoped_nsobject<NSDate> recently_accessed_device_timestamp_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothAdapter::Observer> observers_;

  base::WeakPtrFactory<BluetoothAdapterMac> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_
