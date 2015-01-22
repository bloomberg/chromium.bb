// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_SHUTDOWN_POLICY_OBSERVER_H_
#define ASH_SYSTEM_CHROMEOS_SHUTDOWN_POLICY_OBSERVER_H_

namespace ash {

// Registered with SystemTrayDelegate to observe the |kRebootOnShutdown| policy.
class ASH_EXPORT ShutdownPolicyObserver {
 public:
  // Called when the value of the |kRebootOnShutdown| policy is changed.
  virtual void OnShutdownPolicyChanged(bool reboot_on_shutdown) = 0;

 protected:
  virtual ~ShutdownPolicyObserver() {}
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_SHUTDOWN_POLICY_OBSERVER_H_
