// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/test_accessibility_controller_client.h"

namespace ash {

TestAccessibilityControllerClient::TestAccessibilityControllerClient()
    : binding_(this) {}

TestAccessibilityControllerClient::~TestAccessibilityControllerClient() =
    default;

mojom::AccessibilityControllerClientPtr
TestAccessibilityControllerClient::CreateInterfacePtrAndBind() {
  mojom::AccessibilityControllerClientPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void TestAccessibilityControllerClient::TriggerAccessibilityAlert(
    mojom::AccessibilityAlert alert) {
  last_a11y_alert_ = alert;
}

}  // namespace ash
