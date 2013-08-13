// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_POLICY_OBSERVER_H_
#define CHROMEOS_NETWORK_NETWORK_POLICY_OBSERVER_H_

#include <string>

#include "base/basictypes.h"

namespace chromeos {

class NetworkPolicyObserver {
 public:
  virtual void PolicyApplied(const std::string& service_path) = 0;

 protected:
  virtual ~NetworkPolicyObserver() {};

 private:
  DISALLOW_ASSIGN(NetworkPolicyObserver);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_POLICY_OBSERVER_H_
