// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_H_

#include "base/macros.h"
#include "base/observer_list.h"

namespace chromeos {

namespace multidevice_setup {

// Verifies that this device can connect to the currently-set MultiDevice host.
// In order for a host device to be considered set, its BETTER_TOGETHER_HOST
// software feature must be enabled. In order for a host device to be
// considered verified,
//   * at least one of its other host software features must be enabled, and
//   * the host device's public key, persistent symmetric key, and beacon
//     seeds--data necessary for Bluetooth communication with the MultiDevice
//     client--must be available.
//
// HostVerifier waits for that situation to occur and has the ability (via its
// AttemptVerificationNow() function) to send a tickle message to the phone to
// ask it to enable its software features.
class HostVerifier {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnHostVerified() = 0;
  };

  virtual ~HostVerifier();

  // Returns whether verification for the current MultiDevice host device has
  // completed (see description above). If no MultiDevice host is set at all,
  // false is returned.
  virtual bool IsHostVerified() = 0;

  // Attempts the verification flow; successful completion of the flow is
  // communicated via the OnHostVerified() delegate callback.
  void AttemptVerificationNow();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  HostVerifier();

  virtual void PerformAttemptVerificationNow() = 0;

  void NotifyHostVerified();

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(HostVerifier);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_H_
