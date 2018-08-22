// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_
#define CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/aura/env_observer.h"
#include "ui/display/display_observer.h"
#include "ui/views/accessibility/ax_aura_obj_wrapper.h"

namespace aura {
class Window;
}  // namespace aura

// Provides the root AX desktop node for the chrome.automation.getDesktop() API
// call. Each top-level desktop window is a child.
class AXRootObjWrapper : public views::AXAuraObjWrapper,
                         display::DisplayObserver,
                         aura::EnvObserver {
 public:
  AXRootObjWrapper();
  ~AXRootObjWrapper() override;

  // Returns an AXAuraObjWrapper for an alert window with title set to |text|.
  views::AXAuraObjWrapper* GetAlertForText(const std::string& text);

  // Convenience method to check for existence of a child.
  bool HasChild(views::AXAuraObjWrapper* child);

  // views::AXAuraObjWrapper overrides.
  bool IsIgnored() override;
  views::AXAuraObjWrapper* GetParent() override;
  void GetChildren(
      std::vector<views::AXAuraObjWrapper*>* out_children) override;
  void Serialize(ui::AXNodeData* out_node_data) override;
  const ui::AXUniqueId& GetUniqueId() const override;

 private:
  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;
  void OnWillDestroyEnv() override;

  ui::AXUniqueId unique_id_;

  std::unique_ptr<aura::Window> alert_window_;

  DISALLOW_COPY_AND_ASSIGN(AXRootObjWrapper);
};

#endif  // CHROME_BROWSER_UI_AURA_ACCESSIBILITY_AX_ROOT_OBJ_WRAPPER_H_
