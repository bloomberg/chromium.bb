// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_BLUETOOTH_THROTTLER_H_
#define COMPONENTS_CRYPTAUTH_BLUETOOTH_THROTTLER_H_

namespace base {
class TimeDelta;
}

namespace cryptauth {

class Connection;

// An interface for throttling repeated connection attempts to the same device.
// This throttling is necessary to prevent a kernel race condition when
// connecting before the previous connection fully closes, putting the
// connection in a corrupted, and unrecoverable state. http://crbug.com/345232
class BluetoothThrottler {
 public:
  virtual ~BluetoothThrottler() {}

  // Returns the current delay that must be respected prior to reattempting to
  // establish a connection with the remote device. The returned value is 0 if
  // no delay is needed.
  virtual base::TimeDelta GetDelay() const = 0;

  // Should be called when a connection to the remote device is established.
  // Note that the |connection| is passed as a weak reference. The throttler
  // will ensure, by registering as an observer, that it never attempts to use
  // the connection after it has been destroyed.
  virtual void OnConnection(Connection* connection) = 0;
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_BLUETOOTH_THROTTLER_H_
