// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_PROXIMITY_MONITOR_IMPL_H
#define COMPONENTS_PROXIMITY_AUTH_PROXIMITY_MONITOR_IMPL_H

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/proximity_auth/proximity_monitor.h"
#include "components/proximity_auth/remote_device.h"
#include "device/bluetooth/bluetooth_device.h"

namespace base {
class TickClock;
class TimeTicks;
}

namespace device {
class BluetoothAdapter;
}

namespace proximity_auth {

class ProximityMonitorObserver;

// The concrete implemenation of the proximity monitor interface.
class ProximityMonitorImpl : public ProximityMonitor {
 public:
  // The |observer| is not owned, and must outlive |this| instance.
  ProximityMonitorImpl(const RemoteDevice& remote_device,
                       std::unique_ptr<base::TickClock> clock);
  ~ProximityMonitorImpl() override;

  // ProximityMonitor:
  void Start() override;
  void Stop() override;
  Strategy GetStrategy() const override;
  bool IsUnlockAllowed() const override;
  bool IsInRssiRange() const override;
  void RecordProximityMetricsOnAuthSuccess() override;
  void AddObserver(ProximityMonitorObserver* observer) override;
  void RemoveObserver(ProximityMonitorObserver* observer) override;

 protected:
  // Sets the proximity detection strategy. Exposed for testing.
  // TODO(isherman): Stop exposing this for testing once prefs are properly
  // hooked up.
  virtual void SetStrategy(Strategy strategy);

 private:
  struct TransmitPowerReading {
    TransmitPowerReading(int transmit_power, int max_transmit_power);

    // Returns true if |this| transmit power reading indicates proximity.
    bool IsInProximity() const;

    // The current transmit power.
    int transmit_power;

    // The maximum possible transmit power.
    int max_transmit_power;
  };

  // Callback for asynchronous initialization of the Bluetooth adpater.
  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter);

  // Ensures that the app is periodically polling for the proximity status
  // between the remote and the local device iff it should be, based on the
  // current app state.
  void UpdatePollingState();

  // Performs a scheduled |UpdatePollingState()| operation. This method is
  // used to distinguish periodically scheduled calls to |UpdatePollingState()|
  // from event-driven calls, which should be handled differently.
  void PerformScheduledUpdatePollingState();

  // Returns |true| iff the app should be periodically polling for the proximity
  // status between the remote and the local device.
  bool ShouldPoll() const;

  // Polls the connection information.
  void Poll();

  // Callback to received the polled-for connection info.
  void OnConnectionInfo(
      const device::BluetoothDevice::ConnectionInfo& connection_info);

  // Resets the proximity state to |false|, and clears all member variables
  // tracking the proximity state.
  void ClearProximityState();

  // Updates the proximity state with a new |connection_info| sample of the
  // current RSSI and Tx power, and the device's maximum Tx power.
  void AddSample(
      const device::BluetoothDevice::ConnectionInfo& connection_info);

  // Checks whether the proximity state has changed based on the current
  // samples. Notifies |observers_| on a change.
  void CheckForProximityStateChange();

  // The remote device being monitored.
  const RemoteDevice remote_device_;

  // The observers attached to the ProximityMonitor.
  base::ObserverList<ProximityMonitorObserver> observers_;

  // The Bluetooth adapter that will be polled for connection info.
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  // The strategy used to determine whether the remote device is in proximity.
  Strategy strategy_;

  // Whether the remote device is currently in close proximity to the local
  // device.
  bool remote_device_is_in_proximity_;

  // Whether the proximity monitor is active, i.e. should possibly be scanning
  // for proximity to the remote device.
  bool is_active_;

  // The exponentailly weighted rolling average of the RSSI, used to smooth the
  // RSSI readings. Null if the monitor is inactive, has not recently observed
  // an RSSI reading, or the most recent connection info included an invalid
  // measurement.
  std::unique_ptr<double> rssi_rolling_average_;

  // The last TX power reading. Null if the monitor is inactive, has not
  // recently observed a TX power reading, or the most recent connection info
  // included an invalid measurement.
  std::unique_ptr<TransmitPowerReading> last_transmit_power_reading_;

  // The timestamp of the last zero RSSI reading. An RSSI value of 0 is special
  // because both devices adjust their transmit powers such that the RSSI is in
  // this golden range, if possible. Null if the monitor is inactive, has not
  // recently observed an RSSI reading, or the most recent connection info
  // included an invalid measurement.
  std::unique_ptr<base::TimeTicks> last_zero_rssi_timestamp_;

  // Used to access non-decreasing time measurements.
  std::unique_ptr<base::TickClock> clock_;

  // Used to vend weak pointers for polling. Using a separate factory for these
  // weak pointers allows the weak pointers to be invalidated when polling
  // stops, which effectively cancels the scheduled tasks.
  base::WeakPtrFactory<ProximityMonitorImpl> polling_weak_ptr_factory_;

  // Used to vend all other weak pointers.
  base::WeakPtrFactory<ProximityMonitorImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProximityMonitorImpl);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_PROXIMITY_MONITOR_IMPL_H
