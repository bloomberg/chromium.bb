// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/win/scoped_handle.h"
#include "device/bluetooth/bluetooth_adapter.h"

class MessageLoop;

namespace base {

class SequencedTaskRunner;
class SequencedWorkerPool;

}  // namespace base

namespace device {

// Manages the blocking Bluetooth tasks using |SequencedWorkerPool|. It runs
// bluetooth tasks using |SequencedWorkerPool| and informs its observers of
// bluetooth adapter state changes and any other bluetooth device inquiry
// result.
//
// It delegates the blocking Windows API calls to |bluetooth_task_runner_|'s
// message loop, and receives responses via methods like OnAdapterStateChanged
// posted to UI thread.
class BluetoothTaskManagerWin
    : public base::RefCountedThreadSafe<BluetoothTaskManagerWin> {
 public:
  struct AdapterState {
    std::string name;
    std::string address;
    bool powered;
  };

  class Observer {
   public:
     virtual ~Observer() {}

     virtual void AdapterStateChanged(const AdapterState& state) {}
  };

  BluetoothTaskManagerWin(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void Initialize();
  void Shutdown();

  void PostSetPoweredBluetoothTask(
      bool powered,
      const base::Closure& callback,
      const BluetoothAdapter::ErrorCallback& error_callback);

 private:
  friend class base::RefCountedThreadSafe<BluetoothTaskManagerWin>;
  friend class BluetoothTaskManagerWinTest;

  // Returns true if the machine has a bluetooth stack available. The first call
  // to this function will involve file IO, so it should be done on an
  // appropriate thread. This function is not threadsafe.
  static bool HasBluetoothStack();

  static const int kPollIntervalMs;

  // Constructor to pass |ui_task_runner_| and |bluetooth_task_runner_| for
  // testing.
  BluetoothTaskManagerWin(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner);

  virtual ~BluetoothTaskManagerWin();

  // Notify all Observers of updated AdapterState. Should only be called on the
  // UI thread.
  void OnAdapterStateChanged(const AdapterState* state);

  // Called on BluetoothTaskRunner.
  void StartPolling();
  void PollAdapter();
  void SetPowered(bool powered,
                  const base::Closure& callback,
                  const BluetoothAdapter::ErrorCallback& error_callback);

  // UI task runner reference.
  const scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  const scoped_refptr<base::SequencedWorkerPool> worker_pool_;
  const scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner_;

  // List of observers interested in event notifications.
  ObserverList<Observer> observers_;

  // Adapter handle owned by bluetooth task runner.
  base::win::ScopedHandle adapter_handle_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothTaskManagerWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_