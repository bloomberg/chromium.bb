// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_TEST_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_TEST_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/public/interfaces/accessibility_focus_ring_controller.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

// Test implementation of ash's mojo AccessibilityFocusRingController interface.
//
// Registers itself to ServiceManager on construction and deregisters
// on destruction.
//
// Note: A ServiceManagerConnection must be initialized before constructing this
// object. Consider using content::TestServiceManagerContext on your tests.
class TestAccessibilityFocusRingController
    : public ash::mojom::AccessibilityFocusRingController {
 public:
  TestAccessibilityFocusRingController();
  ~TestAccessibilityFocusRingController() override;

  // ash::mojom::AccessibilityFocusRingController:
  void SetFocusRing(const std::string& focus_ring_id,
                    ash::mojom::FocusRingPtr focus_ring) override;
  void HideFocusRing(const std::string& focus_ring_id) override;
  void SetHighlights(const std::vector<gfx::Rect>& rects_in_screen,
                     uint32_t skcolor) override;
  void HideHighlights() override;

 private:
  void Bind(mojo::ScopedMessagePipeHandle handle);
  mojo::Binding<ash::mojom::AccessibilityFocusRingController> binding_{this};

  DISALLOW_COPY_AND_ASSIGN(TestAccessibilityFocusRingController);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_TEST_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_
