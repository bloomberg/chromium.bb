// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_

#include "components/arc/arc_service.h"
#include "components/arc/common/accessibility_helper.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class ArcBridgeService;

// ArcAccessibilityHelperBridge is an instance to receive converted Android
// accessibility events and info via mojo interface and dispatch them to chrome
// os components.
class ArcAccessibilityHelperBridge
    : public ArcService,
      public mojom::AccessibilityHelperHost,
      public InstanceHolder<mojom::AccessibilityHelperInstance>::Observer {
 public:
  explicit ArcAccessibilityHelperBridge(ArcBridgeService* bridge_service);
  ~ArcAccessibilityHelperBridge() override;

  // InstanceHolder<mojom::AccessibilityHelperInstance>::Observer overrides.
  void OnInstanceReady() override;

  // mojom::AccessibilityHelperHost overrides.
  void OnAccessibilityEvent(
      mojom::AccessibilityEventType event_type,
      mojom::AccessibilityNodeInfoDataPtr event_source) override;

 private:
  mojo::Binding<mojom::AccessibilityHelperHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcAccessibilityHelperBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_ARC_ACCESSIBILITY_HELPER_BRIDGE_H_
