// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STICKY_KEYS_STICKY_KEYS_OVERLAY_H_
#define ASH_STICKY_KEYS_STICKY_KEYS_OVERLAY_H_

#include "ash/ash_export.h"
#include "ash/sticky_keys/sticky_keys_state.h"
#include "base/memory/scoped_ptr.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Rect;
}

namespace views {
class Widget;
}

namespace ash {

class StickyKeysOverlayView;

// Controls the overlay UI for sticky keys, an accessibility feature allowing
// use of modifier keys without holding them down. This overlay will appear as
// a transparent window on the top left of the screen, showing the state of
// each sticky key modifier.
class ASH_EXPORT StickyKeysOverlay : public ui::LayerAnimationObserver {
 public:
  StickyKeysOverlay();
  virtual ~StickyKeysOverlay();

  // Shows or hides the overlay.
  void Show(bool visible);

  void SetModifierVisible(ui::EventFlags modifier, bool visible);

  bool GetModifierVisible(ui::EventFlags modifier);

  // Updates the overlay with the current state of a sticky key modifier.
  void SetModifierKeyState(ui::EventFlags modifier,
                           StickyKeyState state);

  // Get the current state of the sticky key modifier in the overlay.
  StickyKeyState GetModifierKeyState(ui::EventFlags modifier);

  // Returns true if the overlay is currently visible. If the overlay is
  // animating, the returned value is the target of the animation.
  bool is_visible() { return is_visible_; }

 private:
  // Returns the current bounds of the overlay, which is based on visibility.
  gfx::Rect CalculateOverlayBounds();

  // gfx::LayerAnimationObserver overrides:
  virtual void OnLayerAnimationEnded(
      ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) OVERRIDE;

  bool is_visible_;
  scoped_ptr<views::Widget> overlay_widget_;
  // Ownership of |overlay_view_| is passed to the view heirarchy.
  StickyKeysOverlayView* overlay_view_;
  gfx::Size widget_size_;
};

}  // namespace ash

#endif  // ASH_STICKY_KEYS_STICKY_KEYS_OVERLAY_H_
