// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/client_controlled_state.h"

namespace exo {
namespace test {

class TestClientControlledStateDelegate
    : public ash::wm::ClientControlledState::Delegate {
 public:
  static void InstallFactory();
  static void UninstallFactory();

  TestClientControlledStateDelegate();
  ~TestClientControlledStateDelegate() override;

  // Overridden from ash::wm::ClientControlledState::Delegate:
  void HandleWindowStateRequest(
      ash::wm::WindowState* window_state,
      ash::mojom::WindowStateType next_state) override;
  void HandleBoundsRequest(ash::wm::WindowState* window_state,
                           const gfx::Rect& bounds) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestClientControlledStateDelegate);
};

}  // namespace test
}  // namespace exo
