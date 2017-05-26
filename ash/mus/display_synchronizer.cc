// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/display_synchronizer.h"

#include "ash/shell.h"
#include "ui/aura/mus/window_manager_delegate.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"

namespace ash {

DisplaySynchronizer::DisplaySynchronizer(
    aura::WindowManagerClient* window_manager_client)
    : window_manager_client_(window_manager_client) {
  Shell::Get()->window_tree_host_manager()->AddObserver(this);
  SendDisplayConfigurationToServer();
}

DisplaySynchronizer::~DisplaySynchronizer() {
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
}

void DisplaySynchronizer::SendDisplayConfigurationToServer() {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const size_t display_count = display_manager->GetNumDisplays();
  if (display_count == 0)
    return;

  std::vector<display::Display> displays;
  std::vector<ui::mojom::WmViewportMetricsPtr> metrics;
  for (size_t i = 0; i < display_count; ++i) {
    displays.push_back(display_manager->GetDisplayAt(i));
    ui::mojom::WmViewportMetricsPtr viewport_metrics =
        ui::mojom::WmViewportMetrics::New();
    const display::ManagedDisplayInfo& display_info =
        display_manager->GetDisplayInfo(displays.back().id());
    viewport_metrics->bounds_in_pixels = display_info.bounds_in_native();
    viewport_metrics->device_scale_factor = display_info.device_scale_factor();
    viewport_metrics->ui_scale_factor = display_info.configured_ui_scale();
    metrics.push_back(std::move(viewport_metrics));
  }
  window_manager_client_->SetDisplayConfiguration(
      displays, std::move(metrics),
      WindowTreeHostManager::GetPrimaryDisplayId());
}

void DisplaySynchronizer::OnDisplaysInitialized() {
  SendDisplayConfigurationToServer();
}

void DisplaySynchronizer::OnDisplayConfigurationChanged() {
  SendDisplayConfigurationToServer();
}

}  // namespace ash
