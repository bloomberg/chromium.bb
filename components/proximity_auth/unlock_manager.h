// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_UNLOCK_MANAGER_H
#define COMPONENTS_PROXIMITY_AUTH_UNLOCK_MANAGER_H

#include "components/proximity_auth/screenlock_bridge.h"

namespace proximity_auth {

class RemoteDeviceLifeCycle;

// Interface for manager, which controls the lock screen logic.
class UnlockManager {
 public:
  virtual ~UnlockManager() {}

  // Whether proximity-based unlocking is currently allowed. True if any one of
  // the remote devices is authenticated and in range.
  virtual bool IsUnlockAllowed() = 0;

  // Sets the |life_cycle| of the rmeote device to which local events are
  // dispatched. A null |life_cycle| indicates that proximity-based
  // authentication is inactive.
  virtual void SetRemoteDeviceLifeCycle(RemoteDeviceLifeCycle* life_cycle) = 0;

  // Called when the life cycle's state changes.
  virtual void OnLifeCycleStateChanged() = 0;

  // Called when the user pod is clicked for an authentication attempt of type
  // |auth_type|.
  // Exposed for testing.
  virtual void OnAuthAttempted(mojom::AuthType auth_type) = 0;
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_UNLOCK_MANAGER_H
