// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/bluetooth_connection_finder.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/proximity_auth/bluetooth_connection.h"
#include "components/proximity_auth/logging/logging.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

using device::BluetoothAdapter;

namespace proximity_auth {

BluetoothConnectionFinder::BluetoothConnectionFinder(
    const RemoteDevice& remote_device,
    const device::BluetoothUUID& uuid,
    const base::TimeDelta& polling_interval)
    : remote_device_(remote_device),
      uuid_(uuid),
      polling_interval_(polling_interval),
      has_delayed_poll_scheduled_(false),
      weak_ptr_factory_(this) {
}

BluetoothConnectionFinder::~BluetoothConnectionFinder() {
  UnregisterAsObserver();
}

void BluetoothConnectionFinder::Find(
    const ConnectionCallback& connection_callback) {
  if (!device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
    PA_LOG(WARNING) << "Bluetooth is unsupported on this platform. Aborting.";
    return;
  }

  DCHECK(start_time_.is_null());

  start_time_ = base::TimeTicks::Now();
  connection_callback_ = connection_callback;

  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&BluetoothConnectionFinder::OnAdapterInitialized,
                 weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<Connection> BluetoothConnectionFinder::CreateConnection() {
  return std::unique_ptr<Connection>(
      new BluetoothConnection(remote_device_, uuid_));
}

void BluetoothConnectionFinder::SeekDeviceByAddress(
    const std::string& bluetooth_address,
    const base::Closure& callback,
    const bluetooth_util::ErrorCallback& error_callback) {
  bluetooth_util::SeekDeviceByAddress(
      bluetooth_address, callback, error_callback,
      base::ThreadTaskRunnerHandle::Get().get());
}

bool BluetoothConnectionFinder::IsReadyToPoll() {
  bool is_adapter_available =
      adapter_.get() && adapter_->IsPresent() && adapter_->IsPowered();
  PA_LOG(INFO) << "Readiness: adapter="
               << (is_adapter_available ? "available" : "unavailable");
  return is_adapter_available;
}

void BluetoothConnectionFinder::PollIfReady() {
  if (!IsReadyToPoll())
    return;

  // If there is a pending task to poll at a later time, the time requisite
  // timeout has not yet elapsed since the previous polling attempt. In that
  // case, keep waiting until the delayed task comes in.
  if (has_delayed_poll_scheduled_)
    return;

  // If the |connection_| is pending, wait for it to connect or fail prior to
  // polling again.
  if (connection_ && connection_->status() != Connection::DISCONNECTED)
    return;

  // This SeekDeviceByAddress operation is needed to connect to a device if
  // it is not already known to the adapter.
  if (!adapter_->GetDevice(remote_device_.bluetooth_address)) {
    PA_LOG(INFO) << "Remote device [" << remote_device_.bluetooth_address
                 << "] is not known. "
                 << "Seeking device directly by address...";

    SeekDeviceByAddress(
        remote_device_.bluetooth_address,
        base::Bind(&BluetoothConnectionFinder::OnSeekedDeviceByAddress,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&BluetoothConnectionFinder::OnSeekedDeviceByAddressError,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    PA_LOG(INFO) << "Remote device known, connecting...";
    connection_ = CreateConnection();
    connection_->AddObserver(this);
    connection_->Connect();
  }
}

void BluetoothConnectionFinder::PostDelayedPoll() {
  if (has_delayed_poll_scheduled_) {
    PA_LOG(WARNING) << "Delayed poll already scheduled, skipping.";
    return;
  }

  PA_LOG(INFO) << "Posting delayed poll..";
  has_delayed_poll_scheduled_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&BluetoothConnectionFinder::OnDelayedPoll,
                            weak_ptr_factory_.GetWeakPtr()),
      polling_interval_);
}

void BluetoothConnectionFinder::OnDelayedPoll() {
  // Note that there is no longer a pending task, and therefore polling is
  // permitted.
  has_delayed_poll_scheduled_ = false;
  PollIfReady();
}

void BluetoothConnectionFinder::OnSeekedDeviceByAddress() {
  // Sanity check that the remote device is now known by the adapter.
  if (adapter_->GetDevice(remote_device_.bluetooth_address))
    PollIfReady();
  else
    PostDelayedPoll();
}

void BluetoothConnectionFinder::OnSeekedDeviceByAddressError(
    const std::string& error_message) {
  PA_LOG(ERROR) << "Failed to seek device: " << error_message;
  PostDelayedPoll();
}

void BluetoothConnectionFinder::UnregisterAsObserver() {
  if (connection_) {
    connection_->RemoveObserver(this);
    // The connection is about to be released or destroyed, so no need to clear
    // it explicitly here.
  }

  if (adapter_.get()) {
    adapter_->RemoveObserver(this);
    adapter_ = NULL;
  }
}

void BluetoothConnectionFinder::OnAdapterInitialized(
    scoped_refptr<BluetoothAdapter> adapter) {
  adapter_ = adapter;
  adapter_->AddObserver(this);
  PollIfReady();
}

void BluetoothConnectionFinder::AdapterPresentChanged(BluetoothAdapter* adapter,
                                                      bool present) {
  PollIfReady();
}

void BluetoothConnectionFinder::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                                      bool powered) {
  PollIfReady();
}

void BluetoothConnectionFinder::OnConnectionStatusChanged(
    Connection* connection,
    Connection::Status old_status,
    Connection::Status new_status) {
  DCHECK_EQ(connection, connection_.get());

  if (connection_->IsConnected()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - start_time_;
    PA_LOG(WARNING) << "Connection found! Elapsed Time: "
                    << elapsed.InMilliseconds() << "ms.";
    UnregisterAsObserver();

    // If we invoke the callback now, the callback function may install its own
    // observer to |connection_|. Because we are in the ConnectionObserver
    // callstack, this new observer will receive this connection event.
    // Therefore, we need to invoke the callback asynchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&BluetoothConnectionFinder::InvokeCallbackAsync,
                              weak_ptr_factory_.GetWeakPtr()));
  } else if (old_status == Connection::IN_PROGRESS) {
    PA_LOG(WARNING)
        << "Connection failed! Scheduling another polling iteration.";
    PostDelayedPoll();
  }
}

void BluetoothConnectionFinder::InvokeCallbackAsync() {
  connection_callback_.Run(std::move(connection_));
}

}  // namespace proximity_auth
