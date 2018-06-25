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
// The verification process consists of creating a Bluetooth connection to the
// device, performing an authentication handshake, and enabling the per-device
// features which are supported.
class HostVerifier {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnHostVerified() = 0;
  };

  virtual ~HostVerifier();

  // Returns whether the host has completed verification; note that if
  // verification is still in the process of being completed but has not
  // finished, this function still returns false.
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
  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(HostVerifier);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_HOST_VERIFIER_H_
