// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/window_service_util.h"

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/non_client_frame_controller.h"
#include "ash/ws/window_service_delegate_impl.h"
#include "services/ui/ws2/gpu_support.h"
#include "services/ui/ws2/window_service.h"

namespace ash {

std::unique_ptr<service_manager::Service> CreateWindowService(
    const base::RepeatingCallback<std::unique_ptr<ui::ws2::GpuSupport>()>&
        gpu_support_factory) {
  auto window_service = std::make_unique<ui::ws2::WindowService>(
      Shell::Get()->window_service_delegate(), gpu_support_factory.Run());
  window_service->SetFrameDecorationValues(
      NonClientFrameController::GetPreferredClientAreaInsets(),
      NonClientFrameController::GetMaxTitleBarButtonWidth());
  RegisterWindowProperties(window_service->property_converter());
  return window_service;
}

}  // namespace ash
