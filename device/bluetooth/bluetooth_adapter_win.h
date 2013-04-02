// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_

#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"

namespace base {

class SequencedTaskRunner;

}  // namespace base

namespace device {

class BluetoothAdapterFactory;
class BluetoothAdapterWinTest;
class BluetoothDevice;

class BluetoothAdapterWin : public BluetoothAdapter,
                            public BluetoothTaskManagerWin::Observer {
 public:
  typedef base::Callback<void()> InitCallback;

  // BluetoothAdapter override
  virtual void AddObserver(BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual std::string address() const OVERRIDE;
  virtual std::string name() const OVERRIDE;
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

  // BluetoothTaskManagerWin::Observer override
  virtual void AdapterStateChanged(
      const BluetoothTaskManagerWin::AdapterState& state) OVERRIDE;
  virtual void DiscoveryStarted(bool success) OVERRIDE;
  virtual void DiscoveryStopped() OVERRIDE;
  virtual void DevicesDiscovered(
      const ScopedVector<BluetoothTaskManagerWin::DeviceState>& devices)
          OVERRIDE;

 private:
  friend class BluetoothAdapterFactory;
  friend class BluetoothAdapterWinTest;

  enum DiscoveryStatus {
    NOT_DISCOVERING,
    DISCOVERY_STARTING,
    DISCOVERING,
    DISCOVERY_STOPPING
  };

  explicit BluetoothAdapterWin(const InitCallback& init_callback);
  virtual ~BluetoothAdapterWin();

  void Init();
  void InitForTest(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner);

  void MaybePostStartDiscoveryTask();
  void MaybePostStopDiscoveryTask();

  InitCallback init_callback_;
  std::string address_;
  std::string name_;
  bool initialized_;
  bool powered_;
  DiscoveryStatus discovery_status_;

  std::vector<std::pair<base::Closure, ErrorCallback> >
      on_start_discovery_callbacks_;
  std::vector<base::Closure> on_stop_discovery_callbacks_;
  size_t num_discovery_listeners_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<BluetoothTaskManagerWin> task_manager_;

  base::ThreadChecker thread_checker_;

  // List of observers interested in event notifications from us.
  ObserverList<BluetoothAdapter::Observer> observers_;

  // NOTE: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterWin> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_
