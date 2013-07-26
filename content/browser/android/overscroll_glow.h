// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_OVERSCROLL_GLOW_H_
#define CONTENT_BROWSER_ANDROID_OVERSCROLL_GLOW_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/browser/android/edge_effect.h"
#include "ui/gfx/size_f.h"
#include "ui/gfx/vector2d_f.h"

class SkBitmap;

namespace cc {
class Layer;
}

namespace content {

/* |OverscrollGlow| mirrors its Android counterpart, OverscrollGlow.java.
 * Conscious tradeoffs were made to align this as closely as possible with the
 * original Android java version.
 */
class OverscrollGlow {
 public:
  // Create and initialize a new effect with the necessary resources.
  // If |enabled| is false, the effect will be be deactivated until
  // SetEnabled(true) is called.
  // The caller should attach |root_layer| to the desired layer tree.
  static scoped_ptr<OverscrollGlow> Create(bool enabled);

  // Force loading of any necessary resources.  This function is thread-safe.
  static void EnsureResources();

  ~OverscrollGlow();

  // If false, the glow will be deactivated, and subsequent calls to
  // OnOverscrolled or Animate will have no effect.
  void SetEnabled(bool enabled);

  // |overscroll| is the accumulated overscroll for the current gesture.
  // |velocity| is the instantaneous velocity for the overscroll.
  void OnOverscrolled(base::TimeTicks current_time,
                      gfx::Vector2dF overscroll,
                      gfx::Vector2dF velocity);

  // Triggers glow recession for any active edges.
  // Note: This does not actually release any resources; the name mirrors that
  //       in Android's OverscrollGlow class.
  void Release(base::TimeTicks current_time);

  // Returns true if the effect still needs animation ticks.
  bool Animate(base::TimeTicks current_time);

  // Returns true if the effect needs animation ticks.
  bool NeedsAnimate() const;

  // The root layer of the effect (not necessarily of the tree).
  scoped_refptr<cc::Layer> root_layer() const {
    return root_layer_;
  }

  // Horizontal overscroll will be ignored when false.
  void set_horizontal_overscroll_enabled(bool enabled) {
    horizontal_overscroll_enabled_ = enabled;
  }
  // Vertical overscroll will be ignored when false.
  void set_vertical_overscroll_enabled(bool enabled) {
    vertical_overscroll_enabled_ = enabled;
  }
  // The size of the layer for which edges will be animated.
  void set_size(gfx::SizeF size) {
    size_ = size;
  }

 private:
  enum Axis { AXIS_X, AXIS_Y };

  OverscrollGlow(bool enabled, const SkBitmap& edge, const SkBitmap& glow);

  void Pull(base::TimeTicks current_time,
            gfx::Vector2dF added_overscroll);
  void Absorb(base::TimeTicks current_time,
              gfx::Vector2dF velocity,
              gfx::Vector2dF overscroll,
              gfx::Vector2dF old_overscroll);

  void ReleaseAxis(Axis axis, base::TimeTicks current_time);

  EdgeEffect* GetOppositeEdge(int edge_index);

  scoped_ptr<EdgeEffect> edge_effects_[EdgeEffect::EDGE_COUNT];

  bool enabled_;
  gfx::SizeF size_;
  gfx::Vector2dF old_overscroll_;
  gfx::Vector2dF old_velocity_;
  bool horizontal_overscroll_enabled_;
  bool vertical_overscroll_enabled_;

  scoped_refptr<cc::Layer> root_layer_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollGlow);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SCROLL_GLOW_H_
