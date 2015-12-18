// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SETTINGS_ARC_SETTINGS_BRIDGE_H_
#define COMPONENTS_ARC_SETTINGS_ARC_SETTINGS_BRIDGE_H_

namespace arc {

// We want ArcServiceManager to own the ArcSettingsBridge but ArcSettingsBridge
// depends on code in chrome/ which is not accessible from components/.  Since
// ArcSettingsBridge interacts with the bridge through the global Get function,
// session manager only needs to see a limited interface.
//
// Full implementation at chrome/browser/chromeos/arc/arc_settings_bridge_impl.h
class ArcSettingsBridge {
 public:
  virtual ~ArcSettingsBridge() = 0;

  // Starts listening to state changes of the ArcBridgeService
  virtual void StartObservingBridgeServiceChanges() = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SETTINGS_ARC_SETTINGS_BRIDGE_H_
