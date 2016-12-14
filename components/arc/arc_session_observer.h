// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_SESSION_OBSERVER_H_
#define COMPONENTS_ARC_ARC_SESSION_OBSERVER_H_

#include <ostream>

namespace arc {

// Interface to observe the ARC instance running state.
class ArcSessionObserver {
 public:
  // Describes the reason the ARC instance is stopped.
  enum class StopReason {
    // ARC instance has been gracefully shut down.
    SHUTDOWN,

    // Errors occurred during the ARC instance boot. This includes any failures
    // before the instance is actually attempted to be started, and also
    // failures on bootstrapping IPC channels with Android.
    GENERIC_BOOT_FAILURE,

    // The device is critically low on disk space.
    LOW_DISK_SPACE,

    // ARC instance has crashed.
    CRASH,
  };

  virtual ~ArcSessionObserver() = default;

  // Called when the connection with ARC instance has been established.
  virtual void OnSessionReady() {}

  // Called when ARC instance is stopped. This is called exactly once
  // per instance which is Start()ed.
  virtual void OnSessionStopped(StopReason reason) {}
};

// Defines "<<" operator for LOGging purpose.
std::ostream& operator<<(std::ostream& os,
                         ArcSessionObserver::StopReason reason);

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_SESSION_OBSERVER_H_
