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

const char kWidgetName[] = "SwitchAccessMenu";
const int kFocusRingBuffer = 5;
const int kPanelHeight = 200;
const int kPanelWidth = 400;

}  // namespace

// static
const std::string urlForContent = std::string(EXTENSION_PREFIX) +
                                  extension_misc::kSwitchAccessExtensionId +
                                  "/menu_panel.html";

SwitchAccessPanel::SwitchAccessPanel(content::BrowserContext* browser_context)
    : AccessibilityPanel(browser_context, urlForContent, kWidgetName) {
  Hide();
}

void SwitchAccessPanel::Show(const gfx::Rect& element_bounds) {
  // TODO(crbug/893752): Support multiple displays
  gfx::Rect screen_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  gfx::Rect panel_bounds = CalculatePanelBounds(element_bounds, screen_bounds);

  GetAccessibilityController()->SetAccessibilityPanelBounds(
      panel_bounds, ash::mojom::AccessibilityPanelState::BOUNDED);
}

void SwitchAccessPanel::Hide() {
  // This isn't set to (0, 0, 0, 0) because the drop shadow remains visible.
  // TODO(crbug/911344): Find the root cause and fix it.
  gfx::Rect bounds(-1, -1, 1, 1);
  GetAccessibilityController()->SetAccessibilityPanelBounds(
      bounds, ash::mojom::AccessibilityPanelState::BOUNDED);
}

const gfx::Rect SwitchAccessPanel::CalculatePanelBounds(
    const gfx::Rect& element_bounds,
    const gfx::Rect& screen_bounds) {
  gfx::Rect padded_element_bounds = element_bounds;
  padded_element_bounds.Inset(-GetFocusRingBuffer(), -GetFocusRingBuffer());

  // Decide if the horizontal position should be to the right of the element, to
  // the left of the element, or if neither is possible, against the right edge
  // of the screen.
  int panel_x = padded_element_bounds.right();
  if (padded_element_bounds.right() + kPanelWidth > screen_bounds.right()) {
    if (padded_element_bounds.x() - kPanelWidth > screen_bounds.x())
      panel_x = padded_element_bounds.x() - kPanelWidth;
    else
      panel_x = screen_bounds.right() - kPanelWidth;
  }

  // Decide if the vertical position should be below the element, above the
  // element, or if neither is possible, against the bottom edge of the screen.
  int panel_y = padded_element_bounds.bottom();
  if (padded_element_bounds.bottom() + kPanelHeight > screen_bounds.bottom()) {
    if (padded_element_bounds.y() - kPanelHeight > screen_bounds.y())
      panel_y = padded_element_bounds.y() - kPanelHeight;
    else
      panel_y = screen_bounds.bottom() - kPanelHeight;
  }

  return gfx::Rect(panel_x, panel_y, kPanelWidth, kPanelHeight);
}

int SwitchAccessPanel::GetFocusRingBuffer() {
  return kFocusRingBuffer;
}
