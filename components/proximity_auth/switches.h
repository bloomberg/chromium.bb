// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_SWITCHES_H_
#define COMPONENTS_PROXIMITY_AUTH_SWITCHES_H_

namespace proximity_auth {
namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kCryptAuthHTTPHost[];
extern const char kEnableBluetoothLowEnergyDiscovery[];
extern const char kEnableProximityDetection[];
extern const char kForceLoadEasyUnlockAppInTests[];

}  // namespace switches
}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_SWITCHES_H_
