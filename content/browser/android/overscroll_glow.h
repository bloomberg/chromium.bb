// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_OVERSCROLL_GLOW_H_
#define CONTENT_BROWSER_ANDROID_OVERSCROLL_GLOW_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "content/browser/android/edge_effect.h"
#include "ui/gfx/size_f.h"
#include "ui/gfx/vector2d_f.h"

class SkBitmap;

namespace cc {
class Layer;
}

namespace content {

/* |OverscrollGlow| mirrors its Android counterpart, EdgeEffect.java.
 * Conscious tradeoffs were made to align this as closely as possible with the
 * original Android java version.
 */
class OverscrollGlow {
 public:
  static scoped_ptr<OverscrollGlow> Create();

  ~OverscrollGlow();

  void OnOverscrolled(base::TimeTicks current_time,
                      gfx::Vector2dF overscroll,
                      gfx::Vector2dF velocity);
  // Returns true if the effect still needs animation ticks.
  bool Animate(base::TimeTicks current_time);
  void Finish();

  // Returns true if the effect needs animation ticks.
  bool IsActive() const;

  // The root layer of the effect (not necessarily of the tree).
  scoped_refptr<cc::Layer> root_layer() const {
    return root_layer_;
  }

  void set_horizontal_overscroll_enabled(bool enabled) {
    horizontal_overscroll_enabled_ = enabled;
  }
  void set_vertical_overscroll_enabled(bool enabled) {
    vertical_overscroll_enabled_ = enabled;
  }

  void set_size(gfx::SizeF size) {
    size_ = size;
  }

 private:
  enum Axis { AXIS_X, AXIS_Y };

  OverscrollGlow(const SkBitmap& edge, const SkBitmap& glow);

  void Pull(base::TimeTicks current_time,
            gfx::Vector2dF added_overscroll);
  void Absorb(base::TimeTicks current_time,
              gfx::Vector2dF velocity,
              gfx::Vector2dF overscroll,
              gfx::Vector2dF old_overscroll);

  void Release(base::TimeTicks current_time);
  void Release(Axis axis, base::TimeTicks current_time);

  EdgeEffect* GetOppositeEdge(int edge_index);

  scoped_ptr<EdgeEffect> edge_effects_[EdgeEffect::EDGE_COUNT];

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
