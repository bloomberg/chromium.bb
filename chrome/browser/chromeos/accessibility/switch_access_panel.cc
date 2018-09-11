// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/switch_access_panel.h"

#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/views/widget/widget.h"

namespace {

const char kWidgetName[] = "SwitchAccessPanel";
const int kPanelHeight = 50;

}  // namespace

// static
const std::string urlForContent = std::string(EXTENSION_PREFIX) +
                                  extension_misc::kSwitchAccessExtensionId +
                                  "/panel.html";

SwitchAccessPanel::SwitchAccessPanel(content::BrowserContext* browser_context)
    : AccessibilityPanel(browser_context, urlForContent, kWidgetName) {
  gfx::Rect bounds(0, 0, 0 /* overridden */, kPanelHeight);
  GetAccessibilityController()->SetAccessibilityPanelBounds(
      bounds, ash::mojom::AccessibilityPanelState::FULL_WIDTH);
}
