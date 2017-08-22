// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_SCHEDULER_DEVICE_STATUS_LISTENER_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_SCHEDULER_DEVICE_STATUS_LISTENER_H_

#include <memory>

#include "base/power_monitor/power_observer.h"
#include "base/timer/timer.h"
#include "components/download/internal/scheduler/device_status.h"
#include "components/download/internal/scheduler/network_status_listener.h"
#include "net/base/network_change_notifier.h"

namespace download {

// Listens to network and battery status change and notifies the observer.
class DeviceStatusListener : public NetworkStatusListener::Observer,
                             public base::PowerObserver {
 public:
  class Observer {
   public:
    // Called when device status is changed.
    virtual void OnDeviceStatusChanged(const DeviceStatus& device_status) = 0;
  };

  explicit DeviceStatusListener(const base::TimeDelta& delay);
  ~DeviceStatusListener() override;

  // Returns the current device status for download scheduling.
  const DeviceStatus& CurrentDeviceStatus() const;

  // Starts/stops to listen network and battery change events, virtual for
  // testing.
  virtual void Start(DeviceStatusListener::Observer* observer);
  virtual void Stop();

 protected:
  // Creates the instance of |network_listener_|, visible for testing.
  virtual void BuildNetworkStatusListener();

  // Used to listen to network connectivity changes.
  std::unique_ptr<NetworkStatusListener> network_listener_;

  // The current device status.
  DeviceStatus status_;

  // The observer that listens to device status change events.
  Observer* observer_;

  // If we are actively listening to network and battery change events.
  bool listening_;

 private:
  // NetworkStatusListener::Observer implementation.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // base::PowerObserver implementation.
  void OnPowerStateChange(bool on_battery_power) override;

  // Notifies the observer about device status change.
  void NotifyStatusChange();

  // Called after a delay to notify the observer. See |delay_|.
  void NotifyNetworkChangeAfterDelay(NetworkStatus network_status);

  // Used to notify the observer after a delay when network becomes connected.
  base::OneShotTimer timer_;

  // The delay used by |timer_|.
  base::TimeDelta delay_;

  DISALLOW_COPY_AND_ASSIGN(DeviceStatusListener);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_SCHEDULER_DEVICE_STATUS_LISTENER_H_
