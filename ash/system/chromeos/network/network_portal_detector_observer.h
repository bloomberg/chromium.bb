// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_PORTAL_DETECTOR_OBSERVER_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_PORTAL_DETECTOR_OBSERVER_H

#include <string>

namespace ash {

class NetworkPortalDetectorObserver {
 public:
  virtual ~NetworkPortalDetectorObserver() {}

  // Called when captive portal is detected for a |network|.
  virtual void OnCaptivePortalDetected(const std::string& service_path) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_PORTAL_DETECTOR_OBSERVER_H
