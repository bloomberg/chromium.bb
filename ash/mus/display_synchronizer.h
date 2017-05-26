// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_DISPLAY_SYNCHRONIZER_H_
#define ASH_MUS_DISPLAY_SYNCHRONIZER_H_

#include "ash/display/window_tree_host_manager.h"
#include "base/macros.h"

namespace aura {
class WindowManagerClient;
}

namespace ash {

// DisplaySynchronizer keeps the display state in mus in sync with ash's display
// state. As ash controls the overall display state this synchronization is one
// way (from ash to mus).
class DisplaySynchronizer : public ash::WindowTreeHostManager::Observer {
 public:
  explicit DisplaySynchronizer(
      aura::WindowManagerClient* window_manager_client);
  ~DisplaySynchronizer() override;

 private:
  void SendDisplayConfigurationToServer();

  // WindowTreeHostManager::Observer:
  void OnDisplaysInitialized() override;
  void OnDisplayConfigurationChanged() override;

  aura::WindowManagerClient* window_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(DisplaySynchronizer);
};

}  // namespace ash

#endif  // ASH_MUS_DISPLAY_SYNCHRONIZER_H_
