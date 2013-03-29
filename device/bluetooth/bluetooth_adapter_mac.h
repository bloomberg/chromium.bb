// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_

#include <IOKit/IOReturn.h>

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "device/bluetooth/bluetooth_adapter.h"

#ifdef __OBJC__
@class BluetoothAdapterMacDelegate;
@class IOBluetoothDevice;
@class IOBluetoothDeviceInquiry;
@class NSArray;
#else
class BluetoothAdapterMacDelegate;
class IOBluetoothDevice;
class IOBluetoothDeviceInquiry;
class NSArray;
#endif

namespace base {

class SequencedTaskRunner;

}  // namespace base

namespace device {

class BluetoothAdapterMacTest;

class BluetoothAdapterMac : public BluetoothAdapter {
 public:
  // BluetoothAdapter override
  virtual void AddObserver(BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual bool IsInitialized() const OVERRIDE;
  virtual bool IsPresent() const OVERRIDE;
  virtual bool IsPowered() const OVERRIDE;
  virtual void SetPowered(
      bool powered,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsDiscovering() const OVERRIDE;

  virtual void StartDiscovering(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void StopDiscovering(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ReadLocalOutOfBandPairingData(
      const BluetoothOutOfBandPairingDataCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;

  // called by BluetoothAdapterMacDelegate.
  void DeviceInquiryStarted(IOBluetoothDeviceInquiry* inquiry);
  void DeviceFound(IOBluetoothDeviceInquiry* inquiry,
                   IOBluetoothDevice* device);
  void DeviceInquiryComplete(IOBluetoothDeviceInquiry* inquiry,
                             IOReturn error,
                             bool aborted);

 private:
  friend class BluetoothAdapterFactory;
  friend class BluetoothAdapterMacTest;

  enum DiscoveryStatus {
    NOT_DISCOVERING,
    DISCOVERY_STARTING,
    DISCOVERING,
    DISCOVERY_STOPPING
  };

  BluetoothAdapterMac();
  virtual ~BluetoothAdapterMac();

  void TrackDefaultAdapter();
  void TrackTestAdapter(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);
  void PollAdapter();

  // Adds |devices| into |devices_| and notifies observers of the changes.
  // |devices| is an array of pointers to discovered or paired
  // |IOBluetoothDevice| objects.
  void AddDevices(NSArray* devices);

  // Removes devices that used to be paired but are unpaired by the system from
  // |devices_|.
  // |devices| is an array of pointers to paired |IOBluetoothDevice| objects.
  void RemoveUnpairedDevices(NSArray* paired_devices);

  void MaybeStartDeviceInquiry();
  void MaybeStopDeviceInquiry();

  typedef std::vector<std::pair<base::Closure, ErrorCallback> >
      DiscoveryCallbackList;
  void RunCallbacks(const DiscoveryCallbackList& callback_list,
                    bool success) const;

  bool powered_;
  DiscoveryStatus discovery_status_;

  DiscoveryCallbackList on_start_discovery_callbacks_;
  DiscoveryCallbackList on_stop_discovery_callbacks_;
  size_t num_discovery_listeners_;

  BluetoothAdapterMacDelegate* adapter_delegate_;
  IOBluetoothDeviceInquiry* device_inquiry_;

  // A list of discovered device addresses.
  // This list is used to check if the same device is discovered twice during
  // the discovery between consecutive inquiries.
  base::hash_set<std::string> discovered_devices_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothAdapter::Observer> observers_;

  base::WeakPtrFactory<BluetoothAdapterMac> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_MAC_H_
