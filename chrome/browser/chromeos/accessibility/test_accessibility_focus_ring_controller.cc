// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/test_accessibility_focus_ring_controller.h"

#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_filter.h"

TestAccessibilityFocusRingController::TestAccessibilityFocusRingController() {
  CHECK(content::ServiceManagerConnection::GetForProcess())
      << "ServiceManager is uninitialized. Did you forget to create a "
         "content::TestServiceManagerContext?";
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->OverrideBinderForTesting(
          service_manager::ServiceFilter::ByName(ash::mojom::kServiceName),
          ash::mojom::AccessibilityFocusRingController::Name_,
          base::BindRepeating(&TestAccessibilityFocusRingController::Bind,
                              base::Unretained(this)));
}

TestAccessibilityFocusRingController::~TestAccessibilityFocusRingController() {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->ClearBinderOverrideForTesting(
          service_manager::ServiceFilter::ByName(ash::mojom::kServiceName),
          ash::mojom::AccessibilityFocusRingController::Name_);
}

void TestAccessibilityFocusRingController::SetFocusRing(
    const std::string& focus_ring_id,
    ash::mojom::FocusRingPtr focus_ring) {}

void TestAccessibilityFocusRingController::HideFocusRing(
    const std::string& focus_ring_id) {}

void TestAccessibilityFocusRingController::SetHighlights(
    const std::vector<gfx::Rect>& rects_in_screen,
    uint32_t skcolor) {}

void TestAccessibilityFocusRingController::HideHighlights() {}

void TestAccessibilityFocusRingController::Bind(
    mojo::ScopedMessagePipeHandle handle) {
  binding_.Bind(
      ash::mojom::AccessibilityFocusRingControllerRequest(std::move(handle)));
}

