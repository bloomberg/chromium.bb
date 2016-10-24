// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_UI_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/ui/accessibility_cursor_ring_layer.h"
#include "chrome/browser/chromeos/ui/accessibility_focus_ring_layer.h"
#include "ui/gfx/geometry/rect.h"

namespace chromeos {

// AccessibilityFocusRingController handles drawing custom rings around
// the focused object, cursor, and/or caret for accessibility.
class AccessibilityFocusRingController : public FocusRingLayerDelegate {
 public:
  // Get the single instance of this class.
  static AccessibilityFocusRingController* GetInstance();

  enum FocusRingBehavior { FADE_OUT_FOCUS_RING, PERSIST_FOCUS_RING };

  // Draw a focus ring around the given set of rects, in global screen
  // coordinates. Use |focus_ring_behavior| to specify whether the focus
  // ring should persist or fade out.
  void SetFocusRing(const std::vector<gfx::Rect>& rects,
                    FocusRingBehavior focus_ring_behavior);
  void HideFocusRing();

  // Draw a ring around the mouse cursor. It fades out automatically.
  void SetCursorRing(const gfx::Point& location);
  void HideCursorRing();

  // Draw a ring around the text caret. It fades out automatically.
  void SetCaretRing(const gfx::Point& location);
  void HideCaretRing();

  // Don't fade in / out, for testing.
  void SetNoFadeForTesting();

 protected:
  AccessibilityFocusRingController();
  ~AccessibilityFocusRingController() override;

  // Given an unordered vector of bounding rectangles that cover everything
  // that currently has focus, populate a vector of one or more
  // AccessibilityFocusRings that surround the rectangles. Adjacent or
  // overlapping rectangles are combined first. This function is protected
  // so it can be unit-tested.
  void RectsToRings(const std::vector<gfx::Rect>& rects,
                    std::vector<AccessibilityFocusRing>* rings) const;

  virtual int GetMargin() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(AccessibilityFocusRingControllerTest,
                           CursorWorksOnMultipleDisplays);

  // FocusRingLayerDelegate overrides.
  void OnDeviceScaleFactorChanged() override;
  void OnAnimationStep(base::TimeTicks timestamp) override;

  void UpdateFocusRingsFromFocusRects();

  void AnimateFocusRings(base::TimeTicks timestamp);
  void AnimateCursorRing(base::TimeTicks timestamp);
  void AnimateCaretRing(base::TimeTicks timestamp);

  AccessibilityFocusRing RingFromSortedRects(
      const std::vector<gfx::Rect>& rects) const;
  void SplitIntoParagraphShape(
      const std::vector<gfx::Rect>& rects,
      gfx::Rect* top,
      gfx::Rect* middle,
      gfx::Rect* bottom) const;
  bool Intersects(const gfx::Rect& r1, const gfx::Rect& r2) const;

  struct LayerAnimationInfo {
    base::TimeTicks start_time;
    base::TimeTicks change_time;
    base::TimeDelta fade_in_time;
    base::TimeDelta fade_out_time;
    float opacity = 0;
    bool smooth = false;
  };
  void OnLayerChange(LayerAnimationInfo* animation_info);
  void ComputeOpacity(LayerAnimationInfo* animation_info,
                      base::TimeTicks timestamp);

  LayerAnimationInfo focus_animation_info_;
  std::vector<gfx::Rect> focus_rects_;
  std::vector<AccessibilityFocusRing> previous_focus_rings_;
  std::vector<AccessibilityFocusRing> focus_rings_;
  ScopedVector<AccessibilityFocusRingLayer> focus_layers_;
  FocusRingBehavior focus_ring_behavior_ = FADE_OUT_FOCUS_RING;

  LayerAnimationInfo cursor_animation_info_;
  gfx::Point cursor_location_;
  std::unique_ptr<AccessibilityCursorRingLayer> cursor_layer_;

  LayerAnimationInfo caret_animation_info_;
  gfx::Point caret_location_;
  std::unique_ptr<AccessibilityCursorRingLayer> caret_layer_;

  friend struct base::DefaultSingletonTraits<AccessibilityFocusRingController>;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityFocusRingController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_
