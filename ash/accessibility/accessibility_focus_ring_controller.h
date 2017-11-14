// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/accessibility/accessibility_cursor_ring_layer.h"
#include "ash/accessibility/accessibility_focus_ring_layer.h"
#include "ash/accessibility/accessibility_highlight_layer.h"
#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

// AccessibilityFocusRingController handles drawing custom rings around
// the focused object, cursor, and/or caret for accessibility.
// TODO(mash): Provide mojo API for access from chrome.
class ASH_EXPORT AccessibilityFocusRingController
    : public AccessibilityLayerDelegate {
 public:
  // Get the single instance of this class.
  static AccessibilityFocusRingController* GetInstance();

  enum FocusRingBehavior { FADE_OUT_FOCUS_RING, PERSIST_FOCUS_RING };

  // Set the focus ring color, or reset it back to the default.
  void SetFocusRingColor(SkColor color);
  void ResetFocusRingColor();

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

  // Draw a highlight at the given rects, in global screen coordinates.
  // Rects may be overlapping and will be merged into one layer.
  // This looks similar to selecting a region with the cursor, except
  // it is drawn in the foreground rather than behind a text layer.
  void SetHighlights(const std::vector<gfx::Rect>& rects, SkColor color);
  void HideHighlights();

  // Don't fade in / out, for testing.
  void SetNoFadeForTesting();

  AccessibilityCursorRingLayer* cursor_layer_for_testing() {
    return cursor_layer_.get();
  }
  AccessibilityCursorRingLayer* caret_layer_for_testing() {
    return caret_layer_.get();
  }

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
  // AccessibilityLayerDelegate overrides.
  void OnDeviceScaleFactorChanged() override;
  void OnAnimationStep(base::TimeTicks timestamp) override;

  void UpdateFocusRingsFromFocusRects();
  void UpdateHighlightFromHighlightRects();

  void AnimateFocusRings(base::TimeTicks timestamp);
  void AnimateCursorRing(base::TimeTicks timestamp);
  void AnimateCaretRing(base::TimeTicks timestamp);

  AccessibilityFocusRing RingFromSortedRects(
      const std::vector<gfx::Rect>& rects) const;
  void SplitIntoParagraphShape(const std::vector<gfx::Rect>& rects,
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
  std::vector<std::unique_ptr<AccessibilityFocusRingLayer>> focus_layers_;
  FocusRingBehavior focus_ring_behavior_ = FADE_OUT_FOCUS_RING;
  base::Optional<SkColor> focus_ring_color_;

  LayerAnimationInfo cursor_animation_info_;
  gfx::Point cursor_location_;
  std::unique_ptr<AccessibilityCursorRingLayer> cursor_layer_;

  LayerAnimationInfo caret_animation_info_;
  gfx::Point caret_location_;
  std::unique_ptr<AccessibilityCursorRingLayer> caret_layer_;

  std::vector<gfx::Rect> highlight_rects_;
  std::unique_ptr<AccessibilityHighlightLayer> highlight_layer_;
  SkColor highlight_color_;

  friend struct base::DefaultSingletonTraits<AccessibilityFocusRingController>;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityFocusRingController);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_
