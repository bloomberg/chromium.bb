// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_NETWORK_PORTAL_DETECTOR_OBSERVER_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_NETWORK_PORTAL_DETECTOR_OBSERVER_H_

#include <string>

namespace ash {

class NetworkPortalDetectorObserver {
 public:
  virtual ~NetworkPortalDetectorObserver() {}

  // Called when captive portal is detected for the network associated with
  // |guid|.
  virtual void OnCaptivePortalDetected(const std::string& guid) = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_NETWORK_PORTAL_DETECTOR_OBSERVER_H_
