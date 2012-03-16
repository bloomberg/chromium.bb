// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_RESIZE_SHADOW_H_
#define ASH_WM_RESIZE_SHADOW_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/animation/animation_delegate.h"

namespace aura {
class Window;
}
namespace gfx {
class Rect;
}
namespace ui {
class Layer;
class SlideAnimation;
}

namespace ash {
namespace internal {

// A class to render the resize edge effect when the user moves their mouse
// over a sizing edge.  This is just a visual effect; the actual resize is
// handled by the EventFilter.
class ResizeShadow : public ui::AnimationDelegate {
 public:
  ResizeShadow();
  ~ResizeShadow();

  // Initializes the resize effect layers for a given |window|.
  void Init(aura::Window* window);

  // Shows resize effects for one or more edges based on a |hit_test| code, such
  // as HTRIGHT or HTBOTTOMRIGHT.
  void ShowForHitTest(int hit_test);

  // Hides all resize effects.
  void Hide();

  // Updates the effect positions based on the |bounds| of its parent.
  void Layout(const gfx::Rect& bounds);

  // ui::AnimationDelegate overrides:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

 private:
  void InitLayer(ui::Layer* parent, scoped_ptr<ui::Layer>* new_layer);

  void InitAnimation(scoped_ptr<ui::SlideAnimation>* animation);

  // Update the |opacity| for |layer| and sets the layer invisible if it is
  // completely transparent.
  void UpdateLayerOpacity(ui::Layer* layer, int opacity);

  // Layers of type LAYER_SOLID_COLOR for efficiency.
  scoped_ptr<ui::Layer> top_layer_;
  scoped_ptr<ui::Layer> left_layer_;
  scoped_ptr<ui::Layer> right_layer_;
  scoped_ptr<ui::Layer> bottom_layer_;

  // Slide animations for layer opacity.
  // TODO(jamescook): Switch to using ui::ScopedLayerAnimationSettings once
  // solid color layers support that for opacity.
  scoped_ptr<ui::SlideAnimation> top_animation_;
  scoped_ptr<ui::SlideAnimation> left_animation_;
  scoped_ptr<ui::SlideAnimation> right_animation_;
  scoped_ptr<ui::SlideAnimation> bottom_animation_;

  DISALLOW_COPY_AND_ASSIGN(ResizeShadow);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_RESIZE_SHADOW_H_
