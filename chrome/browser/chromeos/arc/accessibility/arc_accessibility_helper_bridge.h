// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_

#include <memory>

#include "components/arc/arc_service.h"
#include "components/arc/common/accessibility_helper.mojom.h"
#include "components/arc/instance_holder.h"
#include "components/exo/wm_helper.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/accessibility/ax_host_delegate.h"

namespace views {

class View;

}  // namespace views

namespace arc {

class ArcBridgeService;
class AXTreeSourceArc;

// ArcAccessibilityHelperBridge is an instance to receive converted Android
// accessibility events and info via mojo interface and dispatch them to chrome
// os components.
class ArcAccessibilityHelperBridge
    : public ArcService,
      public mojom::AccessibilityHelperHost,
      public InstanceHolder<mojom::AccessibilityHelperInstance>::Observer,
      public exo::WMHelper::ActivationObserver,
      public ui::AXHostDelegate {
 public:
  explicit ArcAccessibilityHelperBridge(ArcBridgeService* bridge_service);
  ~ArcAccessibilityHelperBridge() override;

  // InstanceHolder<mojom::AccessibilityHelperInstance>::Observer overrides.
  void OnInstanceReady() override;

  // mojom::AccessibilityHelperHost overrides.
  void OnAccessibilityEventDeprecated(
      mojom::AccessibilityEventType event_type,
      mojom::AccessibilityNodeInfoDataPtr event_source) override;
  void OnAccessibilityEvent(
      mojom::AccessibilityEventDataPtr event_data) override;

 private:
  // exo::WMHelper::ActivationObserver overrides.
  void OnWindowActivated(aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // AXHostDelegate overrides.
  void PerformAction(const ui::AXActionData& data) override;

  mojo::Binding<mojom::AccessibilityHelperHost> binding_;

  std::unique_ptr<AXTreeSourceArc> tree_source_;
  std::unique_ptr<views::View> focus_stealer_;

  DISALLOW_COPY_AND_ASSIGN(ArcAccessibilityHelperBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_
