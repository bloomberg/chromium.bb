// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/proximity_monitor_impl.h"

#include <math.h>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/metrics.h"
#include "components/proximity_auth/proximity_monitor_observer.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

using device::BluetoothDevice;

namespace proximity_auth {

// The time to wait, in milliseconds, between proximity polling iterations.
const int kPollingTimeoutMs = 250;

// The RSSI threshold below which we consider the remote device to not be in
// proximity.
const int kRssiThreshold = -5;

// The weight of the most recent RSSI sample.
const double kRssiSampleWeight = 0.3;

ProximityMonitorImpl::ProximityMonitorImpl(
    const RemoteDevice& remote_device,
    std::unique_ptr<base::TickClock> clock)
    : remote_device_(remote_device),
      strategy_(Strategy::NONE),
      remote_device_is_in_proximity_(false),
      is_active_(false),
      clock_(std::move(clock)),
      polling_weak_ptr_factory_(this),
      weak_ptr_factory_(this) {
  if (device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable()) {
    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&ProximityMonitorImpl::OnAdapterInitialized,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    PA_LOG(ERROR) << "[Proximity] Proximity monitoring unavailable: "
                  << "Bluetooth is unsupported on this platform.";
  }

  // TODO(isherman): Test prefs to set the strategy. Need to read from "Local
  // State" prefs on the sign-in screen, and per-user prefs on the lock screen.
  // TODO(isherman): Unlike in the JS app, destroy and recreate the proximity
  // monitor when the connection state changes.
}

ProximityMonitorImpl::~ProximityMonitorImpl() {
}

void ProximityMonitorImpl::Start() {
  is_active_ = true;
  UpdatePollingState();
}

void ProximityMonitorImpl::Stop() {
  is_active_ = false;
  ClearProximityState();
  UpdatePollingState();
}

ProximityMonitor::Strategy ProximityMonitorImpl::GetStrategy() const {
  return strategy_;
}

bool ProximityMonitorImpl::IsUnlockAllowed() const {
  return strategy_ == Strategy::NONE || remote_device_is_in_proximity_;
}

bool ProximityMonitorImpl::IsInRssiRange() const {
  return (strategy_ != Strategy::NONE && rssi_rolling_average_ &&
          *rssi_rolling_average_ > kRssiThreshold);
}

void ProximityMonitorImpl::RecordProximityMetricsOnAuthSuccess() {
  double rssi_rolling_average = rssi_rolling_average_
                                    ? *rssi_rolling_average_
                                    : metrics::kUnknownProximityValue;

  int last_transmit_power_delta =
      last_transmit_power_reading_
          ? (last_transmit_power_reading_->transmit_power -
             last_transmit_power_reading_->max_transmit_power)
          : metrics::kUnknownProximityValue;

  // If no zero RSSI value has been read, then record an overflow.
  base::TimeDelta time_since_last_zero_rssi;
  if (last_zero_rssi_timestamp_)
    time_since_last_zero_rssi = clock_->NowTicks() - *last_zero_rssi_timestamp_;
  else
    time_since_last_zero_rssi = base::TimeDelta::FromDays(100);

  std::string remote_device_model = metrics::kUnknownDeviceModel;
  if (remote_device_.name != remote_device_.bluetooth_address)
    remote_device_model = remote_device_.name;

  metrics::RecordAuthProximityRollingRssi(round(rssi_rolling_average));
  metrics::RecordAuthProximityTransmitPowerDelta(last_transmit_power_delta);
  metrics::RecordAuthProximityTimeSinceLastZeroRssi(time_since_last_zero_rssi);
  metrics::RecordAuthProximityRemoteDeviceModelHash(remote_device_model);
}

void ProximityMonitorImpl::AddObserver(ProximityMonitorObserver* observer) {
  observers_.AddObserver(observer);
}

void ProximityMonitorImpl::RemoveObserver(ProximityMonitorObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ProximityMonitorImpl::SetStrategy(Strategy strategy) {
  if (strategy_ == strategy)
    return;
  strategy_ = strategy;
  CheckForProximityStateChange();
  UpdatePollingState();
}

ProximityMonitorImpl::TransmitPowerReading::TransmitPowerReading(
    int transmit_power,
    int max_transmit_power)
    : transmit_power(transmit_power), max_transmit_power(max_transmit_power) {
}

bool ProximityMonitorImpl::TransmitPowerReading::IsInProximity() const {
  return transmit_power < max_transmit_power;
}

void ProximityMonitorImpl::OnAdapterInitialized(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  UpdatePollingState();
}

void ProximityMonitorImpl::UpdatePollingState() {
  if (ShouldPoll()) {
    // If there is a polling iteration already scheduled, wait for it.
    if (polling_weak_ptr_factory_.HasWeakPtrs())
      return;

    // Polling can re-entrantly call back into this method, so make sure to
    // schedule the next polling iteration prior to executing the current one.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ProximityMonitorImpl::PerformScheduledUpdatePollingState,
                   polling_weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kPollingTimeoutMs));
    Poll();
  } else {
    polling_weak_ptr_factory_.InvalidateWeakPtrs();
    remote_device_is_in_proximity_ = false;
  }
}

void ProximityMonitorImpl::PerformScheduledUpdatePollingState() {
  polling_weak_ptr_factory_.InvalidateWeakPtrs();
  UpdatePollingState();
}

bool ProximityMonitorImpl::ShouldPoll() const {
  // Note: We poll even if the strategy is NONE so we can record measurements.
  return is_active_ && bluetooth_adapter_;
}

void ProximityMonitorImpl::Poll() {
  DCHECK(ShouldPoll());

  BluetoothDevice* device =
      bluetooth_adapter_->GetDevice(remote_device_.bluetooth_address);

  if (!device) {
    PA_LOG(ERROR) << "Unknown Bluetooth device with address "
                  << remote_device_.bluetooth_address;
    ClearProximityState();
    return;
  }
  if (!device->IsConnected()) {
    PA_LOG(ERROR) << "Bluetooth device with address "
                  << remote_device_.bluetooth_address << " is not connected.";
    ClearProximityState();
    return;
  }

  device->GetConnectionInfo(base::Bind(&ProximityMonitorImpl::OnConnectionInfo,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void ProximityMonitorImpl::OnConnectionInfo(
    const BluetoothDevice::ConnectionInfo& connection_info) {
  if (!is_active_) {
    PA_LOG(INFO) << "[Proximity] Got connection info after stopping";
    return;
  }

  if (connection_info.rssi != BluetoothDevice::kUnknownPower &&
      connection_info.transmit_power != BluetoothDevice::kUnknownPower &&
      connection_info.max_transmit_power != BluetoothDevice::kUnknownPower) {
    AddSample(connection_info);
  } else {
    PA_LOG(WARNING) << "[Proximity] Unkown values received from API: "
                    << connection_info.rssi << " "
                    << connection_info.transmit_power << " "
                    << connection_info.max_transmit_power;
    rssi_rolling_average_.reset();
    last_transmit_power_reading_.reset();
    CheckForProximityStateChange();
  }
}

void ProximityMonitorImpl::ClearProximityState() {
  if (is_active_ && remote_device_is_in_proximity_) {
    FOR_EACH_OBSERVER(ProximityMonitorObserver, observers_,
                      OnProximityStateChanged());
  }

  remote_device_is_in_proximity_ = false;
  rssi_rolling_average_.reset();
  last_transmit_power_reading_.reset();
  last_zero_rssi_timestamp_.reset();
}

void ProximityMonitorImpl::AddSample(
    const BluetoothDevice::ConnectionInfo& connection_info) {
  double weight = kRssiSampleWeight;
  if (!rssi_rolling_average_) {
    rssi_rolling_average_.reset(new double(connection_info.rssi));
  } else {
    *rssi_rolling_average_ =
        weight * connection_info.rssi + (1 - weight) * (*rssi_rolling_average_);
  }
  last_transmit_power_reading_.reset(new TransmitPowerReading(
      connection_info.transmit_power, connection_info.max_transmit_power));

  // It's rare but possible for the RSSI to be positive briefly.
  if (connection_info.rssi >= 0)
    last_zero_rssi_timestamp_.reset(new base::TimeTicks(clock_->NowTicks()));

  CheckForProximityStateChange();
}

void ProximityMonitorImpl::CheckForProximityStateChange() {
  if (strategy_ == Strategy::NONE)
    return;

  bool is_now_in_proximity = false;
  switch (strategy_) {
    case Strategy::NONE:
      return;

    case Strategy::CHECK_RSSI:
      is_now_in_proximity = IsInRssiRange();
      break;

    case Strategy::CHECK_TRANSMIT_POWER:
      is_now_in_proximity = (last_transmit_power_reading_ &&
                             last_transmit_power_reading_->IsInProximity());
      break;
  }

  if (remote_device_is_in_proximity_ != is_now_in_proximity) {
    PA_LOG(INFO) << "[Proximity] Updated proximity state: "
                 << (is_now_in_proximity ? "proximate" : "distant");
    remote_device_is_in_proximity_ = is_now_in_proximity;
    FOR_EACH_OBSERVER(ProximityMonitorObserver, observers_,
                      OnProximityStateChanged());
  }
}

}  // namespace proximity_auth
