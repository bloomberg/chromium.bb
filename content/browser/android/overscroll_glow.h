// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_OVERSCROLL_GLOW_H_
#define CONTENT_BROWSER_ANDROID_OVERSCROLL_GLOW_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/size_f.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {
class Layer;
}

namespace content {

class EdgeEffectBase;

/* |OverscrollGlow| mirrors its Android counterpart, OverscrollGlow.java.
 * Conscious tradeoffs were made to align this as closely as possible with the
 * original Android Java version.
 */
class OverscrollGlow {
 public:
  enum Edge { EDGE_TOP = 0, EDGE_LEFT, EDGE_BOTTOM, EDGE_RIGHT, EDGE_COUNT };

  // Allows lazy creation of the edge effects.
  typedef base::Callback<scoped_ptr<EdgeEffectBase>(void)> EdgeEffectProvider;

  // |edge_effect_provider| must be valid for the duration of the effect's
  // lifetime.  The effect is enabled by default, but will remain dormant until
  // the first overscroll event.
  explicit OverscrollGlow(const EdgeEffectProvider& edge_effect_provider);

  ~OverscrollGlow();

  // Enable the effect. If the effect was previously disabled, it will remain
  // dormant until subsequent calls to |OnOverscrolled()|.
  void Enable();

  // Deactivate and detach the effect. Subsequent calls to |OnOverscrolled()| or
  // |Animate()| will have no effect.
  void Disable();

  // Effect layers will be attached to |overscrolling_layer| if necessary.
  // |accumulated_overscroll| and |overscroll_delta| are in device pixels, while
  // |velocity| is in device pixels / second.
  // Returns true if the effect still needs animation ticks.
  bool OnOverscrolled(cc::Layer* overscrolling_layer,
                      base::TimeTicks current_time,
                      gfx::Vector2dF accumulated_overscroll,
                      gfx::Vector2dF overscroll_delta,
                      gfx::Vector2dF velocity,
                      gfx::Vector2dF overscroll_location);

  // Returns true if the effect still needs animation ticks.
  // Note: The effect will detach itself when no further animation is required.
  bool Animate(base::TimeTicks current_time);

  // Update the effect according to the most recent display parameters,
  // Note: All dimensions are in device pixels.
  struct DisplayParameters {
    DisplayParameters();
    gfx::SizeF size;
    float edge_offsets[EDGE_COUNT];
  };
  void UpdateDisplayParameters(const DisplayParameters& params);

 private:
  enum Axis { AXIS_X, AXIS_Y };

  // Returns whether the effect is initialized.
  bool InitializeIfNecessary();
  bool NeedsAnimate() const;
  void UpdateLayerAttachment(cc::Layer* parent);
  void Detach();
  void Pull(base::TimeTicks current_time,
            const gfx::Vector2dF& overscroll_delta,
            const gfx::Vector2dF& overscroll_location);
  void Absorb(base::TimeTicks current_time,
              const gfx::Vector2dF& velocity,
              bool x_overscroll_started,
              bool y_overscroll_started);
  void Release(base::TimeTicks current_time);

  EdgeEffectBase* GetOppositeEdge(int edge_index);

  EdgeEffectProvider edge_effect_provider_;
  scoped_ptr<EdgeEffectBase> edge_effects_[EDGE_COUNT];

  DisplayParameters display_params_;
  bool enabled_;
  bool initialized_;

  scoped_refptr<cc::Layer> root_layer_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollGlow);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SCROLL_GLOW_H_
