// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_UI_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/ui/accessibility_cursor_ring_layer.h"
#include "chrome/browser/chromeos/ui/accessibility_focus_ring_layer.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
class Compositor;
}

namespace chromeos {

// AccessibilityFocusRingController handles drawing custom rings around
// the focused object, cursor, and/or caret for accessibility.
class AccessibilityFocusRingController
    : public FocusRingLayerDelegate,
      public ui::CompositorAnimationObserver {
 public:
  // Get the single instance of this class.
  static AccessibilityFocusRingController* GetInstance();

  // Draw a focus ring around the given set of rects, in global screen
  // coordinates.
  void SetFocusRing(const std::vector<gfx::Rect>& rects);

  // Draw a ring around the mouse cursor.
  void SetCursorRing(const gfx::Point& location);

  // Draw a ring around the text caret.
  void SetCaretRing(const gfx::Point& location);

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
  // FocusRingLayerDelegate.
  void OnDeviceScaleFactorChanged() override;

  // CompositorAnimationObserver.
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  void Update();

  void AnimateFocusRings(base::TimeTicks timestamp);
  void AnimateCursorRing(base::TimeTicks timestamp);

  AccessibilityFocusRing RingFromSortedRects(
      const std::vector<gfx::Rect>& rects) const;
  void SplitIntoParagraphShape(
      const std::vector<gfx::Rect>& rects,
      gfx::Rect* top,
      gfx::Rect* middle,
      gfx::Rect* bottom) const;
  bool Intersects(const gfx::Rect& r1, const gfx::Rect& r2) const;
  ui::Compositor* CompositorForBounds(const gfx::Rect& bounds);
  void RemoveAnimationObservers();
  void AddAnimationObservers();

  std::vector<gfx::Rect> rects_;
  std::vector<AccessibilityFocusRing> previous_rings_;
  std::vector<AccessibilityFocusRing> rings_;
  ScopedVector<AccessibilityFocusRingLayer> layers_;
  base::TimeTicks focus_change_time_;
  ui::Compositor* compositor_;

  base::TimeTicks cursor_start_time_;
  base::TimeTicks cursor_change_time_;
  gfx::Point cursor_location_;
  float cursor_opacity_;
  std::unique_ptr<AccessibilityCursorRingLayer> cursor_layer_;
  ui::Compositor* cursor_compositor_;

  gfx::Point caret_location_;
  std::unique_ptr<AccessibilityCursorRingLayer> caret_layer_;

  friend struct base::DefaultSingletonTraits<AccessibilityFocusRingController>;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityFocusRingController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_
