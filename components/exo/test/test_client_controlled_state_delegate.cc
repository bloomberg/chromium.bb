// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/test_client_controlled_state_delegate.h"

#include "ash/wm/window_state.h"
#include "components/exo/client_controlled_shell_surface.h"

namespace exo {
namespace test {

TestClientControlledStateDelegate::TestClientControlledStateDelegate() =
    default;
TestClientControlledStateDelegate::~TestClientControlledStateDelegate() =
    default;

void TestClientControlledStateDelegate::HandleWindowStateRequest(
    ash::wm::WindowState* window_state,
    ash::mojom::WindowStateType next_state) {
  ash::wm::ClientControlledState* state_impl =
      static_cast<ash::wm::ClientControlledState*>(
          ash::wm::WindowState::TestApi::GetStateImpl(window_state));
  state_impl->EnterNextState(window_state, next_state);
}

void TestClientControlledStateDelegate::HandleBoundsRequest(
    ash::wm::WindowState* window_state,
    const gfx::Rect& bounds) {
  ash::wm::ClientControlledState* state_impl =
      static_cast<ash::wm::ClientControlledState*>(
          ash::wm::WindowState::TestApi::GetStateImpl(window_state));
  state_impl->set_bounds_locally(true);
  window_state->window()->SetBounds(bounds);
  state_impl->set_bounds_locally(false);
}

// static
void TestClientControlledStateDelegate::InstallFactory() {
  ClientControlledShellSurface::SetClientControlledStateDelegateFactoryForTest(
      base::BindRepeating([]() {
        return base::WrapUnique<ash::wm::ClientControlledState::Delegate>(
            new TestClientControlledStateDelegate());
      }));
};

// static
void TestClientControlledStateDelegate::UninstallFactory() {
  ClientControlledShellSurface::SetClientControlledStateDelegateFactoryForTest(
      base::RepeatingCallback<
          std::unique_ptr<ash::wm::ClientControlledState::Delegate>(void)>());
}

}  // namespace test
}  // namespace exo
