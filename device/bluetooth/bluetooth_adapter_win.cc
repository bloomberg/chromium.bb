// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(youngki): Implement this file.

#include "device/bluetooth/bluetooth_adapter_win.h"

#include <string>
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"

namespace device {

BluetoothAdapterWin::BluetoothAdapterWin(const InitCallback& init_callback)
    : BluetoothAdapter(),
      init_callback_(init_callback),
      initialized_(false),
      powered_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

BluetoothAdapterWin::~BluetoothAdapterWin() {
  if (task_manager_) {
    task_manager_->RemoveObserver(this);
    task_manager_->Shutdown();
  }
}

void BluetoothAdapterWin::AddObserver(BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothAdapterWin::RemoveObserver(BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

// TODO(youngki): Return true when |task_manager_| initializes the adapter
// state.
bool BluetoothAdapterWin::IsInitialized() const {
  return initialized_;
}

bool BluetoothAdapterWin::IsPresent() const {
  return !address_.empty();
}

bool BluetoothAdapterWin::IsPowered() const {
  return powered_;
}

void BluetoothAdapterWin::SetPowered(
    bool powered,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  task_manager_->PostSetPoweredBluetoothTask(powered, callback, error_callback);
}

bool BluetoothAdapterWin::IsDiscovering() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothAdapterWin::SetDiscovering(
    bool discovering,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

BluetoothAdapter::ConstDeviceList BluetoothAdapterWin::GetDevices() const {
  NOTIMPLEMENTED();
  return BluetoothAdapter::ConstDeviceList();
}

BluetoothDevice* BluetoothAdapterWin::GetDevice(const std::string& address) {
  NOTIMPLEMENTED();
  return NULL;
}

const BluetoothDevice* BluetoothAdapterWin::GetDevice(
    const std::string& address) const {
  NOTIMPLEMENTED();
  return NULL;
}

void BluetoothAdapterWin::ReadLocalOutOfBandPairingData(
    const BluetoothOutOfBandPairingDataCallback& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

void BluetoothAdapterWin::AdapterStateChanged(
    const BluetoothTaskManagerWin::AdapterState& state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  name_ = state.name;
  address_ = state.address;
  powered_ = state.powered;
  if (!initialized_) {
    init_callback_.Run();
  }
  initialized_ = true;
}

void BluetoothAdapterWin::TrackDefaultAdapter() {
  task_manager_ =
      new BluetoothTaskManagerWin(base::ThreadTaskRunnerHandle::Get());
  task_manager_->AddObserver(this);
  task_manager_->Initialize();
}

}  // namespace device
