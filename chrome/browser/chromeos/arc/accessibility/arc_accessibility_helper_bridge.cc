// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "components/arc/arc_bridge_service.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace arc {

ArcAccessibilityHelperBridge::ArcAccessibilityHelperBridge(
    ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->accessibility_helper()->AddObserver(this);
}

ArcAccessibilityHelperBridge::~ArcAccessibilityHelperBridge() {
  arc_bridge_service()->accessibility_helper()->RemoveObserver(this);
}

void ArcAccessibilityHelperBridge::OnInstanceReady() {
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service()->accessibility_helper(), Init);
  DCHECK(instance);
  instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcAccessibilityHelperBridge::OnAccessibilityEvent(
    mojom::AccessibilityEventType event_type,
    mojom::AccessibilityNodeInfoDataPtr event_source) {
  if (event_type != mojom::AccessibilityEventType::VIEW_FOCUSED ||
      event_source.is_null()) {
    return;
  }

  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();

  if (!accessibility_manager ||
      !accessibility_manager->IsFocusHighlightEnabled()) {
    return;
  }

  exo::WMHelper* wmHelper = exo::WMHelper::GetInstance();

  aura::Window* focused_window = wmHelper->GetFocusedWindow();
  if (!focused_window)
    return;

  aura::Window* toplevel_window = focused_window->GetToplevelWindow();

  gfx::Rect bounds_in_screen = gfx::ScaleToEnclosingRect(
      event_source.get()->boundsInScreen,
      1.0f / toplevel_window->layer()->device_scale_factor());

  accessibility_manager->OnViewFocusedInArc(bounds_in_screen);
}

}  // namespace arc
