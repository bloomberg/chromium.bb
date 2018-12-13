// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/switch_access_panel.h"

#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace {

const char kWidgetName[] = "SwitchAccessPanel";
const int kFocusRingBuffer = 5;
const int kPanelHeight = 200;
const int kPanelWidth = 400;

}  // namespace

// static
const std::string urlForContent = std::string(EXTENSION_PREFIX) +
                                  extension_misc::kSwitchAccessExtensionId +
                                  "/panel.html";

SwitchAccessPanel::SwitchAccessPanel(content::BrowserContext* browser_context)
    : AccessibilityPanel(browser_context, urlForContent, kWidgetName) {
  Hide();
}

void SwitchAccessPanel::Show(const gfx::Rect& element_bounds) {
  // Show the menu off the lower right corner of the object.
  int x = element_bounds.x() + element_bounds.width() + kFocusRingBuffer;
  int y = element_bounds.y() + element_bounds.height() + kFocusRingBuffer;

  // TODO(crbug/893752): Support multiple displays
  gfx::Rect screen_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  x = std::min(x, screen_bounds.x() + screen_bounds.width() - kPanelWidth);
  y = std::min(y, screen_bounds.y() + screen_bounds.height() - kPanelHeight);

  gfx::Rect bounds(x, y, kPanelWidth, kPanelHeight);
  GetAccessibilityController()->SetAccessibilityPanelBounds(
      bounds, ash::mojom::AccessibilityPanelState::BOUNDED);
}

void SwitchAccessPanel::Hide() {
  // This isn't set to (0, 0, 0, 0) because the drop shadow remains visible.
  // TODO(crbug/911344): Find the root cause and fix it.
  gfx::Rect bounds(-1, -1, 1, 1);
  GetAccessibilityController()->SetAccessibilityPanelBounds(
      bounds, ash::mojom::AccessibilityPanelState::BOUNDED);
}
