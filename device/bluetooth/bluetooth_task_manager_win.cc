// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_task_manager_win.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/win/scoped_handle.h"
#include "device/bluetooth/bluetooth_init_win.h"

namespace {

const int kNumThreadsInWorkerPool = 3;
const char kBluetoothThreadName[] = "BluetoothPollingThreadWin";

// Populates bluetooth adapter state using adapter_handle.
void GetAdapterState(HANDLE adapter_handle,
                     device::BluetoothTaskManagerWin::AdapterState* state) {
  std::string name;
  std::string address;
  bool powered = false;
  BLUETOOTH_RADIO_INFO adapter_info = { sizeof(BLUETOOTH_RADIO_INFO), 0 };
  if (adapter_handle &&
      ERROR_SUCCESS == BluetoothGetRadioInfo(adapter_handle,
                                             &adapter_info)) {
    name = base::SysWideToUTF8(adapter_info.szName);
    address = base::StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X",
        adapter_info.address.rgBytes[5],
        adapter_info.address.rgBytes[4],
        adapter_info.address.rgBytes[3],
        adapter_info.address.rgBytes[2],
        adapter_info.address.rgBytes[1],
        adapter_info.address.rgBytes[0]);
    powered = !!BluetoothIsConnectable(adapter_handle);
  }
  state->name = name;
  state->address = address;
  state->powered = powered;
}

}

namespace device {

// static
const int BluetoothTaskManagerWin::kPollIntervalMs = 500;

BluetoothTaskManagerWin::BluetoothTaskManagerWin(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ui_task_runner_(ui_task_runner),
      worker_pool_(new base::SequencedWorkerPool(kNumThreadsInWorkerPool,
                                                 kBluetoothThreadName)),
      bluetooth_task_runner_(
          worker_pool_->GetSequencedTaskRunnerWithShutdownBehavior(
              worker_pool_->GetSequenceToken(),
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN)) {
}

BluetoothTaskManagerWin::BluetoothTaskManagerWin(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner)
    : ui_task_runner_(ui_task_runner),
      bluetooth_task_runner_(bluetooth_task_runner) {
}

BluetoothTaskManagerWin::~BluetoothTaskManagerWin() {
}

void BluetoothTaskManagerWin::AddObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  observers_.AddObserver(observer);
}

void BluetoothTaskManagerWin::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  observers_.RemoveObserver(observer);
}

void BluetoothTaskManagerWin::Initialize() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  bluetooth_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::StartPolling,
                 this));
}

void BluetoothTaskManagerWin::StartPolling() {
  DCHECK(bluetooth_task_runner_->RunsTasksOnCurrentThread());

  // TODO(youngki): Handle this check where BluetoothAdapter is initialized.
  if (device::bluetooth_init_win::HasBluetoothStack()) {
    PollAdapter();
  } else {
    // IF the bluetooth stack is not available, we still send an empty state
    // to BluetoothAdapter so that it is marked initialized, but the adapter
    // will not be present.
    AdapterState* state = new AdapterState();
    ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::OnAdapterStateChanged,
                 this,
                 base::Owned(state)));
  }
}

void BluetoothTaskManagerWin::Shutdown() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  worker_pool_->Shutdown();
}

void BluetoothTaskManagerWin::PostSetPoweredBluetoothTask(
    bool powered,
    const base::Closure& callback,
    const BluetoothAdapter::ErrorCallback& error_callback) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  bluetooth_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::SetPowered,
                 this,
                 powered,
                 callback,
                 error_callback));
}

void BluetoothTaskManagerWin::OnAdapterStateChanged(const AdapterState* state) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  FOR_EACH_OBSERVER(BluetoothTaskManagerWin::Observer, observers_,
                    AdapterStateChanged(*state));
}

void BluetoothTaskManagerWin::PollAdapter() {
  DCHECK(bluetooth_task_runner_->RunsTasksOnCurrentThread());

  const BLUETOOTH_FIND_RADIO_PARAMS adapter_param =
      { sizeof(BLUETOOTH_FIND_RADIO_PARAMS) };
  if (adapter_handle_)
    adapter_handle_.Close();
  HBLUETOOTH_RADIO_FIND handle = BluetoothFindFirstRadio(
      &adapter_param, adapter_handle_.Receive());

  if (handle)
    BluetoothFindRadioClose(handle);

  AdapterState* state = new AdapterState();
  GetAdapterState(adapter_handle_, state);

  // Notify the UI thread of the Bluetooth Adapter state.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::OnAdapterStateChanged,
                 this,
                 base::Owned(state)));

  // Re-poll.
  bluetooth_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::PollAdapter,
                 this),
      base::TimeDelta::FromMilliseconds(kPollIntervalMs));
}

void BluetoothTaskManagerWin::SetPowered(
    bool powered,
    const base::Closure& callback,
    const BluetoothAdapter::ErrorCallback& error_callback) {
  DCHECK(bluetooth_task_runner_->RunsTasksOnCurrentThread());
  bool success = false;
  if (adapter_handle_) {
    if (!powered)
      BluetoothEnableDiscovery(adapter_handle_, false);
    success = !!BluetoothEnableIncomingConnections(adapter_handle_, powered);
  }

  if (success) {
    AdapterState* state = new AdapterState();
    GetAdapterState(adapter_handle_, state);
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BluetoothTaskManagerWin::OnAdapterStateChanged,
                   this,
                   base::Owned(state)));
    ui_task_runner_->PostTask(FROM_HERE, callback);
  } else {
    ui_task_runner_->PostTask(FROM_HERE, error_callback);
  }
}

}  // namespace device
