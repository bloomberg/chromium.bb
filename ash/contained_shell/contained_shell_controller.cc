// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/contained_shell/contained_shell_controller.h"

#include <utility>

namespace ash {

ContainedShellController::ContainedShellController() = default;

ContainedShellController::~ContainedShellController() = default;

void ContainedShellController::BindRequest(
    mojom::ContainedShellControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ContainedShellController::LaunchContainedShell() {
  // TODO(crbug/902571): Implement launch by dispatching to a
  // ContainedShellClient method.
  NOTIMPLEMENTED();
}

void ContainedShellController::SetClient(
    mojom::ContainedShellClientPtr client) {
  contained_shell_client_ = std::move(client);
}

}  // namespace ash
