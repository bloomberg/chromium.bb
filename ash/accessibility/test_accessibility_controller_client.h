// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_TEST_ACCESSIBILITY_CONTROLLER_CLEINT_H_
#define ASH_ACCESSIBILITY_TEST_ACCESSIBILITY_CONTROLLER_CLEINT_H_

#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

// Implement AccessibilityControllerClient mojo interface to simulate chrome
// behavior in tests. This breaks the ash/chrome dependency to allow testing ash
// code in isolation.
class TestAccessibilityControllerClient
    : public mojom::AccessibilityControllerClient {
 public:
  TestAccessibilityControllerClient();
  ~TestAccessibilityControllerClient() override;

  mojom::AccessibilityControllerClientPtr CreateInterfacePtrAndBind();

  // mojom::AccessibilityControllerClient:
  void TriggerAccessibilityAlert(mojom::AccessibilityAlert alert) override;

  mojom::AccessibilityAlert last_a11y_alert() const { return last_a11y_alert_; }

 private:
  mojom::AccessibilityAlert last_a11y_alert_ = mojom::AccessibilityAlert::NONE;

  mojo::Binding<mojom::AccessibilityControllerClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestAccessibilityControllerClient);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_TEST_ACCESSIBILITY_CONTROLLER_CLEINT_H_
