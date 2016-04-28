// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_WINDOW_MANAGER_ARC_WINDOW_MANAGER_BRIDGE_H_
#define COMPONENTS_ARC_WINDOW_MANAGER_ARC_WINDOW_MANAGER_BRIDGE_H_

#include <string>

#include "ash/shell_observer.h"
#include "base/macros.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"

namespace arc {

class ArcWindowManagerBridge : public ArcService,
                               public ArcBridgeService::Observer,
                               public ash::ShellObserver {
 public:
  explicit ArcWindowManagerBridge(ArcBridgeService* bridge_service);
  ~ArcWindowManagerBridge() override;

  // ArcBridgeService::Observer
  void OnWindowManagerInstanceReady() override;

  // Ash::Shell::ShellObserver
  void OnMaximizeModeStarted() override;
  void OnMaximizeModeEnded() override;

 private:
  void SendWindowManagerModeChange(bool touch_view_enabled);

  // Remembers the currently set mode on the Android side.
  mojom::WindowManagerMode current_mode_;

  DISALLOW_COPY_AND_ASSIGN(ArcWindowManagerBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_WINDOW_MANAGER_ARC_WINDOW_MANAGER_BRIDGE_H_
