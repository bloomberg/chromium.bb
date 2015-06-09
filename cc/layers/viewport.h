// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_VIEWPORT_H_
#define CC_LAYERS_VIEWPORT_H_

#include "base/memory/scoped_ptr.h"
#include "cc/layers/layer_impl.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class LayerTreeHostImpl;

// Encapsulates gesture handling logic on the viewport layers. The "viewport"
// is made up of two scrolling layers, the inner viewport (visual) and the
// outer viewport (layout) scroll layers. These layers have different scroll
// bubbling behavior from the rest of the layer tree which is encoded in this
// class.
class CC_EXPORT Viewport {
 public:
  struct ScrollResult {
    gfx::Vector2dF applied_delta;
    gfx::Vector2dF unused_scroll_delta;
    gfx::Vector2dF top_controls_applied_delta;
  };

  static scoped_ptr<Viewport> Create(LayerTreeHostImpl* host_impl);

  // Differs from scrolling in that only the visual viewport is moved, without
  // affecting the top controls or outer viewport.
  void Pan(const gfx::Vector2dF& delta);

  // Scrolls the viewport, applying the unique bubbling between the inner and
  // outer viewport. Scrolls can be consumed by top controls.
  ScrollResult ScrollBy(const gfx::Vector2dF& delta,
                        const gfx::Point& viewport_point,
                        bool is_wheel_scroll,
                        bool affect_top_controls);

 private:
  explicit Viewport(LayerTreeHostImpl* host_impl);

  bool ShouldTopControlsConsumeScroll(const gfx::Vector2dF& scroll_delta) const;
  gfx::Vector2dF AdjustOverscroll(const gfx::Vector2dF& delta) const;

  // Sends the delta to the top controls, returns the amount applied.
  gfx::Vector2dF ScrollTopControls(const gfx::Vector2dF& delta);

  gfx::ScrollOffset MaxTotalScrollOffset() const;
  gfx::ScrollOffset TotalScrollOffset() const;

  LayerImpl* InnerScrollLayer() const;
  LayerImpl* OuterScrollLayer() const;

  LayerTreeHostImpl* host_impl_;

  DISALLOW_COPY_AND_ASSIGN(Viewport);
};

}  // namespace cc

#endif  // CC_LAYERS_VIEWPORT_H_
