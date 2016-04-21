// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_THROTTLER_IMPL_H
#define COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_THROTTLER_IMPL_H

#include <memory>
#include <set>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/proximity_auth/bluetooth_throttler.h"
#include "components/proximity_auth/connection_observer.h"

namespace base {
class TickClock;
}

namespace proximity_auth {

class Connection;

// This class throttles repeated connection attempts to the same device. This
// throttling is necessary to prevent a kernel race condition when connecting
// before the previous connection fully closes, putting the connection in a
// corrupted, and unrecoverable state. http://crbug.com/345232
class BluetoothThrottlerImpl : public BluetoothThrottler,
                               public ConnectionObserver {
 public:
  // Creates a throttler for connections to a remote device, using the |clock|
  // as a time source.
  explicit BluetoothThrottlerImpl(std::unique_ptr<base::TickClock> clock);
  ~BluetoothThrottlerImpl() override;

  // BluetoothThrottler:
  base::TimeDelta GetDelay() const override;
  void OnConnection(Connection* connection) override;

 protected:
  // Returns the duration to wait, after disconnecting, before reattempting a
  // connection to the remote device. Exposed for testing.
  base::TimeDelta GetCooldownTimeDelta() const;

 private:
  // ConnectionObserver:
  void OnConnectionStatusChanged(Connection* connection,
                                 Connection::Status old_status,
                                 Connection::Status new_status) override;

  // Tracks the last seen disconnect time for the |remote_device_|.
  base::TimeTicks last_disconnect_time_;

  // The time source.
  std::unique_ptr<base::TickClock> clock_;

  // The currently connected connections.
  // Each connection is stored as a weak reference, which is safe because |this|
  // instance is registered as an observer, and will unregister when the
  // connection disconnects, which is guaranteed to occur before the connection
  // is destroyed.
  std::set<Connection*> connections_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothThrottlerImpl);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_BLUETOOTH_THROTTLER_IMPL_H
