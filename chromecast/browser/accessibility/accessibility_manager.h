// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_MANAGER_H_
#define CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_MANAGER_H_

#include <memory>
#include <vector>

#include "chromecast/browser/accessibility/accessibility_sound_proxy.h"
#include "chromecast/browser/accessibility/touch_exploration_manager.h"
#include "chromecast/graphics/accessibility/accessibility_focus_ring_controller.h"
#include "chromecast/graphics/gestures/multiple_tap_detector.h"

namespace aura {
class WindowTreeHost;
}  // namespace aura

namespace chromecast {

class CastWindowManagerAura;
class FocusRingController;
class MagnificationController;

namespace shell {

// Responsible for delegating chromecast browser process accessibility functions
// to the responsible party.
class AccessibilityManager : public MultipleTapDetectorDelegate {
 public:
  explicit AccessibilityManager(CastWindowManagerAura* window_manager);
  ~AccessibilityManager() override;

  // Sets the focus ring color.
  void SetFocusRingColor(SkColor color);

  // Resets the focus ring color back to the default.
  void ResetFocusRingColor();

  // Draws a focus ring around the given set of rects in screen coordinates. Use
  // |focus_ring_behavior| to specify whether the focus ring should persist or
  // fade out.
  void SetFocusRing(const std::vector<gfx::Rect>& rects_in_screen,
                    FocusRingBehavior focus_ring_behavior);

  // Hides focus ring on screen.
  void HideFocusRing();

  // Draws a highlight at the given rects in screen coordinates. Rects may be
  // overlapping and will be merged into one layer. This looks similar to
  // selecting a region with the cursor, except it is drawn in the foreground
  // rather than behind a text layer.
  void SetHighlights(const std::vector<gfx::Rect>& rects_in_screen,
                     SkColor color);

  // Hides highlight on screen.
  void HideHighlights();

  // Enable or disable screen reader support, including touch exploration.
  void SetScreenReader(bool enable);

  // Update the touch exploration controller so that synthesized
  // touch events are anchored at this point.
  void SetTouchAccessibilityAnchorPoint(const gfx::Point& anchor_point);

  // Get the window tree host this AccessibilityManager was created with.
  aura::WindowTreeHost* window_tree_host() const;

  // Enable or disable the triple-tap gesture to turn on magnification.
  void SetMagnificationGestureEnabled(bool enabled);

  // Returns whether the magnification gesture is currently enabled.
  bool IsMagnificationGestureEnabled() const;

  // MultipleTapDetectorDelegate implementation
  void OnTripleTap(const gfx::Point& tap_location) override;

  // Sets the player for earcons.
  void SetAccessibilitySoundPlayer(
      std::unique_ptr<AccessibilitySoundPlayer> player);

 private:
  aura::WindowTreeHost* window_tree_host_;

  std::unique_ptr<FocusRingController> focus_ring_controller_;
  std::unique_ptr<AccessibilityFocusRingController>
      accessibility_focus_ring_controller_;
  std::unique_ptr<TouchExplorationManager> touch_exploration_manager_;
  std::unique_ptr<MultipleTapDetector> magnify_gesture_detector_;
  std::unique_ptr<MagnificationController> magnification_controller_;

  AccessibilitySoundProxy accessibility_sound_proxy_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ACCESSIBILITY_ACCESSIBILITY_MANAGER_H_
