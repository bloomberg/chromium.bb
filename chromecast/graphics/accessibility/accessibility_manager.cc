// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/accessibility/accessibility_manager.h"

#include "chromecast/graphics/accessibility/focus_ring_controller.h"

namespace chromecast {

AccessibilityManager::AccessibilityManager() {}
AccessibilityManager::~AccessibilityManager() {}

void AccessibilityManager::Setup(aura::Window* root_window,
                                 wm::ActivationClient* activation_client) {
  focus_ring_controller_ =
      std::make_unique<FocusRingController>(root_window, activation_client);
  accessibility_focus_ring_controller_ =
      std::make_unique<AccessibilityFocusRingController>(root_window);
}

void AccessibilityManager::SetFocusRingColor(SkColor color) {
  if (accessibility_focus_ring_controller_)
    accessibility_focus_ring_controller_->SetFocusRingColor(color);
}

void AccessibilityManager::ResetFocusRingColor() {
  if (accessibility_focus_ring_controller_)
    accessibility_focus_ring_controller_->ResetFocusRingColor();
}

void AccessibilityManager::SetFocusRing(
    const std::vector<gfx::Rect>& rects_in_screen,
    FocusRingBehavior focus_ring_behavior) {
  if (accessibility_focus_ring_controller_)
    accessibility_focus_ring_controller_->SetFocusRing(rects_in_screen,
                                                       focus_ring_behavior);
}

void AccessibilityManager::HideFocusRing() {
  if (accessibility_focus_ring_controller_)
    accessibility_focus_ring_controller_->HideFocusRing();
}

void AccessibilityManager::SetHighlights(
    const std::vector<gfx::Rect>& rects_in_screen,
    SkColor color) {
  if (accessibility_focus_ring_controller_)
    accessibility_focus_ring_controller_->SetHighlights(rects_in_screen, color);
}

void AccessibilityManager::HideHighlights() {
  if (accessibility_focus_ring_controller_)
    accessibility_focus_ring_controller_->HideHighlights();
}

void AccessibilityManager::SetTouchAccessibilityAnchorPoint(
    const gfx::Point& anchor_point) {
  // TODO(rdaum): Implement
}

}  // namespace chromecast
