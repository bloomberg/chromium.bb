// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_THROTTLED_BLUETOOTH_CONNECTION_FINDER_H
#define COMPONENTS_PROXIMITY_AUTH_THROTTLED_BLUETOOTH_CONNECTION_FINDER_H

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/proximity_auth/connection_finder.h"

namespace base {
class TaskRunner;
}

namespace proximity_auth {

class BluetoothConnectionFinder;
class BluetoothThrottler;
class Connection;

// A Bluetooth connection finder that delays Find() requests according to the
// throttler's cooldown period.
class ThrottledBluetoothConnectionFinder : public ConnectionFinder {
 public:
  // Note: The |throttler| is not owned, and must outlive |this| instance.
  ThrottledBluetoothConnectionFinder(
      std::unique_ptr<BluetoothConnectionFinder> connection_finder,
      scoped_refptr<base::TaskRunner> task_runner,
      BluetoothThrottler* throttler);
  ~ThrottledBluetoothConnectionFinder() override;

  // ConnectionFinder:
  void Find(const ConnectionCallback& connection_callback) override;

 private:
  // Callback to be called when a connection is found.
  void OnConnection(const ConnectionCallback& connection_callback,
                    std::unique_ptr<Connection> connection);

  // The underlying connection finder.
  std::unique_ptr<BluetoothConnectionFinder> connection_finder_;

  // The task runner used for posting delayed messages.
  scoped_refptr<base::TaskRunner> task_runner_;

  // The throttler managing this connection finder. The throttler is not owned,
  // and must outlive |this| instance.
  BluetoothThrottler* throttler_;

  base::WeakPtrFactory<ThrottledBluetoothConnectionFinder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThrottledBluetoothConnectionFinder);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_THROTTLED_BLUETOOTH_CONNECTION_FINDER_H
