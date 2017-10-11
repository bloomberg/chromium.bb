// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/scheduler/device_status_listener.h"

#include "base/memory/ptr_util.h"
#include "base/power_monitor/power_monitor.h"
#include "build/build_config.h"
#include "components/download/internal/scheduler/network_status_listener.h"

#if defined(OS_ANDROID)
#include "components/download/internal/android/network_status_listener_android.h"
#endif

namespace download {

namespace {

// Converts |on_battery_power| to battery status.
BatteryStatus ToBatteryStatus(bool on_battery_power) {
  return on_battery_power ? BatteryStatus::NOT_CHARGING
                          : BatteryStatus::CHARGING;
}

// Converts a ConnectionType to NetworkStatus.
NetworkStatus ToNetworkStatus(net::NetworkChangeNotifier::ConnectionType type) {
  switch (type) {
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return NetworkStatus::UNMETERED;
    case net::NetworkChangeNotifier::CONNECTION_2G:
    case net::NetworkChangeNotifier::CONNECTION_3G:
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return NetworkStatus::METERED;
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
    case net::NetworkChangeNotifier::CONNECTION_NONE:
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return NetworkStatus::DISCONNECTED;
  }
  NOTREACHED();
  return NetworkStatus::DISCONNECTED;
}

}  // namespace

DeviceStatusListener::DeviceStatusListener(const base::TimeDelta& startup_delay,
                                           const base::TimeDelta& online_delay)
    : observer_(nullptr),
      listening_(false),
      startup_delay_(startup_delay),
      online_delay_(online_delay) {}

DeviceStatusListener::~DeviceStatusListener() {
  Stop();
}

const DeviceStatus& DeviceStatusListener::CurrentDeviceStatus() const {
  return status_;
}

void DeviceStatusListener::Start(DeviceStatusListener::Observer* observer) {
  if (listening_)
    return;

  DCHECK(observer);
  observer_ = observer;

  // Network stack may shake off all connections after getting the IP address,
  // use a delay to wait for potential network setup.
  timer_.Start(FROM_HERE, startup_delay_,
               base::Bind(&DeviceStatusListener::StartAfterDelay,
                          base::Unretained(this)));
}

void DeviceStatusListener::StartAfterDelay() {
  // Listen to battery status changes.
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  DCHECK(power_monitor);
  power_monitor->AddObserver(this);

  // Listen to network status changes.
  BuildNetworkStatusListener();
  network_listener_->Start(this);

  status_.battery_status =
      ToBatteryStatus(base::PowerMonitor::Get()->IsOnBatteryPower());
  status_.network_status =
      ToNetworkStatus(network_listener_->GetConnectionType());
  listening_ = true;

  // Notify the current status if we are online or charging.
  // TODO(xingliu): Do we need to notify if we are disconnected?
  if (status_ != DeviceStatus())
    NotifyStatusChange();
}

void DeviceStatusListener::Stop() {
  timer_.Stop();

  if (!listening_)
    return;

  base::PowerMonitor::Get()->RemoveObserver(this);

  network_listener_->Stop();
  network_listener_.reset();

  status_ = DeviceStatus();
  listening_ = false;
  observer_ = nullptr;
}

void DeviceStatusListener::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  NetworkStatus new_network_status = ToNetworkStatus(type);
  if (status_.network_status == new_network_status)
    return;

  bool change_to_online =
      (status_.network_status == NetworkStatus::DISCONNECTED) &&
      (new_network_status != NetworkStatus::DISCONNECTED);

  // It's unreliable to send requests immediately after the network becomes
  // online that the signal may not fully consider DHCP. Notify network change
  // to the observer after a delay.
  // Android network change listener still need this delay.
  if (change_to_online) {
    timer_.Start(FROM_HERE, online_delay_,
                 base::Bind(&DeviceStatusListener::NotifyNetworkChange,
                            base::Unretained(this), new_network_status));
  } else {
    status_.network_status = new_network_status;
    timer_.Stop();
    NotifyStatusChange();
  }
}

void DeviceStatusListener::OnPowerStateChange(bool on_battery_power) {
  status_.battery_status = ToBatteryStatus(on_battery_power);
  NotifyStatusChange();
}

void DeviceStatusListener::NotifyStatusChange() {
  DCHECK(observer_);
  observer_->OnDeviceStatusChanged(status_);
}

void DeviceStatusListener::NotifyNetworkChange(NetworkStatus network_status) {
  status_.network_status = network_status;
  NotifyStatusChange();
}

void DeviceStatusListener::BuildNetworkStatusListener() {
#if defined(OS_ANDROID)
  network_listener_ = base::MakeUnique<NetworkStatusListenerAndroid>();
#else
  network_listener_ = base::MakeUnique<NetworkStatusListenerImpl>();
#endif
}

}  // namespace download
