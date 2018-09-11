// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SWITCH_ACCESS_PANEL_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SWITCH_ACCESS_PANEL_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/accessibility/accessibility_panel.h"
#include "chrome/common/extensions/extension_constants.h"

// Shows a context menu of controls for Switch Access users
class SwitchAccessPanel : public AccessibilityPanel {
 public:
  explicit SwitchAccessPanel(content::BrowserContext* browser_context);
  ~SwitchAccessPanel() override = default;

  DISALLOW_COPY_AND_ASSIGN(SwitchAccessPanel);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SWITCH_ACCESS_PANEL_H_
