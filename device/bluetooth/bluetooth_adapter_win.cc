// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TODO(youngki): Implement this file.

#include "device/bluetooth/bluetooth_adapter_win.h"

#include <hash_set>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "device/bluetooth/bluetooth_device_win.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"

namespace device {

BluetoothAdapterWin::BluetoothAdapterWin(const InitCallback& init_callback)
    : BluetoothAdapter(),
      init_callback_(init_callback),
      initialized_(false),
      powered_(false),
      discovery_status_(NOT_DISCOVERING),
      scanning_(false),
      num_discovery_listeners_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

BluetoothAdapterWin::~BluetoothAdapterWin() {
  if (task_manager_) {
    task_manager_->RemoveObserver(this);
    task_manager_->Shutdown();
  }
  STLDeleteValues(&devices_);
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
  return discovery_status_ == DISCOVERING ||
      discovery_status_ == DISCOVERY_STOPPING;
}

bool BluetoothAdapterWin::IsScanning() const {
  return scanning_;
}

// If the method is called when |discovery_status_| is DISCOVERY_STOPPING,
// starting again is handled by BluetoothAdapterWin::DiscoveryStopped().
void BluetoothAdapterWin::StartDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (discovery_status_ == DISCOVERING) {
    num_discovery_listeners_++;
    callback.Run();
    return;
  }
  on_start_discovery_callbacks_.push_back(
      std::make_pair(callback, error_callback));
  MaybePostStartDiscoveryTask();
}

void BluetoothAdapterWin::StopDiscovering(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (discovery_status_ == NOT_DISCOVERING) {
    error_callback.Run();
    return;
  }
  on_stop_discovery_callbacks_.push_back(callback);
  MaybePostStopDiscoveryTask();
}

void BluetoothAdapterWin::DiscoveryStarted(bool success) {
  discovery_status_ = success ? DISCOVERING : NOT_DISCOVERING;
  for (std::vector<std::pair<base::Closure, ErrorCallback> >::const_iterator
       iter = on_start_discovery_callbacks_.begin();
       iter != on_start_discovery_callbacks_.end();
       ++iter) {
    if (success)
      ui_task_runner_->PostTask(FROM_HERE, iter->first);
    else
      ui_task_runner_->PostTask(FROM_HERE, iter->second);
  }
  num_discovery_listeners_ = on_start_discovery_callbacks_.size();
  on_start_discovery_callbacks_.clear();

  if (success) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterDiscoveringChanged(this, true));

    // If there are stop discovery requests, post the stop discovery again.
    MaybePostStopDiscoveryTask();
  } else if (!on_stop_discovery_callbacks_.empty()) {
    // If there are stop discovery requests but start discovery has failed,
    // notify that stop discovery has been complete.
    DiscoveryStopped();
  }
}

void BluetoothAdapterWin::DiscoveryStopped() {
  bool was_discovering = IsDiscovering();
  discovery_status_ = NOT_DISCOVERING;
  for (std::vector<base::Closure>::const_iterator iter =
           on_stop_discovery_callbacks_.begin();
       iter != on_stop_discovery_callbacks_.end();
       ++iter) {
    ui_task_runner_->PostTask(FROM_HERE, *iter);
  }
  num_discovery_listeners_ = 0;
  on_stop_discovery_callbacks_.clear();
  ScanningChanged(false);
  if (was_discovering)
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterDiscoveringChanged(this, false));

  // If there are start discovery requests, post the start discovery again.
  MaybePostStartDiscoveryTask();
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
    initialized_ = true;
    init_callback_.Run();
  }
}

void BluetoothAdapterWin::ScanningChanged(bool scanning) {
  if (scanning_ != scanning) {
    scanning_ = scanning;
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterScanningChanged(this, scanning_));
  }
}

void BluetoothAdapterWin::DevicesDiscovered(
    const ScopedVector<BluetoothTaskManagerWin::DeviceState>& devices) {
  std::hash_set<std::string> device_address_list;
  for (ScopedVector<BluetoothTaskManagerWin::DeviceState>::const_iterator iter =
           devices.begin();
       iter != devices.end();
       ++iter) {
    device_address_list.insert((*iter)->address);
    DevicesMap::iterator found_device_iter = devices_.find((*iter)->address);

    if (found_device_iter == devices_.end()) {
      devices_[(*iter)->address] = new BluetoothDeviceWin(**iter);
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceAdded(this, devices_[(*iter)->address]));
      continue;
    }
    BluetoothDeviceWin* device_win =
        static_cast<BluetoothDeviceWin*>(found_device_iter->second);
    if (device_win->device_fingerprint() !=
        BluetoothDeviceWin::ComputeDeviceFingerprint(**iter)) {
      devices_[(*iter)->address] = new BluetoothDeviceWin(**iter);
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceChanged(this, devices_[(*iter)->address]));
      delete device_win;
    }
  }

  DevicesMap::iterator device_iter = devices_.begin();
  while (device_iter != devices_.end()) {
    if (device_address_list.find(device_iter->first) !=
        device_address_list.end()) {
      ++device_iter;
      continue;
    }
    if (device_iter->second->IsConnected() || device_iter->second->IsPaired()) {
      BluetoothDeviceWin* device_win =
          static_cast<BluetoothDeviceWin*>(device_iter->second);
      device_win->SetVisible(false);
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceChanged(this, device_win));
      ++device_iter;
      continue;
    }
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceRemoved(this, device_iter->second));
    delete device_iter->second;
    device_iter = devices_.erase(device_iter);
  }
}

void BluetoothAdapterWin::TrackDefaultAdapter() {
  ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  task_manager_ =
      new BluetoothTaskManagerWin(ui_task_runner_);
  task_manager_->AddObserver(this);
  task_manager_->Initialize();
}

void BluetoothAdapterWin::TrackTestAdapter(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner) {
  ui_task_runner_ = ui_task_runner;
  task_manager_ =
      new BluetoothTaskManagerWin(ui_task_runner_);
  task_manager_->AddObserver(this);
  task_manager_->InitializeWithBluetoothTaskRunner(bluetooth_task_runner);
}

void BluetoothAdapterWin::MaybePostStartDiscoveryTask() {
  if (discovery_status_ == NOT_DISCOVERING &&
      !on_start_discovery_callbacks_.empty()) {
    discovery_status_ = DISCOVERY_STARTING;
    task_manager_->PostStartDiscoveryTask();
  }
}

void BluetoothAdapterWin::MaybePostStopDiscoveryTask() {
  if (discovery_status_ != DISCOVERING)
    return;

  if (on_stop_discovery_callbacks_.size() < num_discovery_listeners_) {
    for (std::vector<base::Closure>::const_iterator iter =
             on_stop_discovery_callbacks_.begin();
         iter != on_stop_discovery_callbacks_.end();
         ++iter) {
      ui_task_runner_->PostTask(FROM_HERE, *iter);
    }
    num_discovery_listeners_ -= on_stop_discovery_callbacks_.size();
    on_stop_discovery_callbacks_.clear();
    return;
  }

  discovery_status_ = DISCOVERY_STOPPING;
  task_manager_->PostStopDiscoveryTask();
}

}  // namespace device
