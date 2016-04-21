// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_CONNECTION_FINDER_H
#define COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_CONNECTION_FINDER_H

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/proximity_auth/bluetooth_util.h"
#include "components/proximity_auth/connection_finder.h"
#include "components/proximity_auth/connection_observer.h"
#include "components/proximity_auth/remote_device.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace proximity_auth {

class BluetoothConnection;

// This ConnectionFinder implementation tries to find a Bluetooth connection to
// the remote device by polling at a fixed interval.
class BluetoothConnectionFinder : public ConnectionFinder,
                                  public ConnectionObserver,
                                  public device::BluetoothAdapter::Observer {
 public:
  BluetoothConnectionFinder(const RemoteDevice& remote_device,
                            const device::BluetoothUUID& uuid,
                            const base::TimeDelta& polling_interval);
  ~BluetoothConnectionFinder() override;

  // ConnectionFinder:
  void Find(const ConnectionCallback& connection_callback) override;

 protected:
  // Exposed for mocking out the connection in tests.
  virtual std::unique_ptr<Connection> CreateConnection();

  // Calls bluetooth_util::SeekDeviceByAddress. Exposed for testing, as this
  // utility function is platform dependent.
  virtual void SeekDeviceByAddress(
      const std::string& bluetooth_address,
      const base::Closure& callback,
      const bluetooth_util::ErrorCallback& error_callback);

  // BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

 private:
  // Returns true iff the Bluetooth adapter is ready to make connections.
  bool IsReadyToPoll();

  // Attempts to connect to the |remote_device_| if the system is ready for
  // another iteration of polling.
  void PollIfReady();

  // Posts a delayed task to call |PollIfReady|. |OnDelayedPoll()| will be
  // called when the task fires.
  void PostDelayedPoll();
  void OnDelayedPoll();

  // Callbacks for bluetooth_util::SeekDeviceByAddress().
  void OnSeekedDeviceByAddress();
  void OnSeekedDeviceByAddressError(const std::string& error_message);

  // Unregisters |this| instance as an observer from all objects that it might
  // have registered with.
  void UnregisterAsObserver();

  // Callback to be called when the Bluetooth adapter is initialized.
  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter);

  // ConnectionObserver:
  void OnConnectionStatusChanged(Connection* connection,
                                 Connection::Status old_status,
                                 Connection::Status new_status) override;

  // Used to invoke |connection_callback_| asynchronously, decoupling the
  // callback invocation from the ConnectionObserver callstack.
  void InvokeCallbackAsync();

  // The remote device to connect to.
  const RemoteDevice remote_device_;

  // The UUID of the service on the remote device.
  const device::BluetoothUUID uuid_;

  // The time to wait between polling attempts.
  const base::TimeDelta polling_interval_;

  // Records the time at which the finder began searching for connections.
  base::TimeTicks start_time_;

  // The callback that should be called upon a successful connection.
  ConnectionCallback connection_callback_;

  // The Bluetooth adapter over which the Bluetooth connection will be made.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // The Bluetooth connection that will be opened.
  std::unique_ptr<Connection> connection_;

  // Whether there is currently a polling task scheduled.
  bool has_delayed_poll_scheduled_;

  // Used to schedule everything else.
  base::WeakPtrFactory<BluetoothConnectionFinder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothConnectionFinder);
};

// TODO(isherman): Make sure to wire up the controller to listen for screen lock
// state change events, and create or destroy the connection finder as
// appropriate.

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_CONNECTION_FINDER_H
