// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/resize_shadow.h"

#include "ash/wm/image_grid.h"
#include "base/time.h"
#include "grit/ash_resources.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace {

// Final opacity for resize effect.
const float kShadowTargetOpacity = 0.25f;
// Animation time for resize effect in milliseconds.
const int kShadowAnimationDurationMs = 200;

// Sets up a layer as invisible and fully transparent, without animating.
void InitLayer(ui::Layer* layer) {
  layer->SetVisible(false);
  layer->SetOpacity(0.f);
}

// Triggers an opacity animation that will make |layer| become |visible|.
void ShowLayer(ui::Layer* layer, bool visible) {
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kShadowAnimationDurationMs));
  layer->SetOpacity(visible ? kShadowTargetOpacity : 0.f);
  // Sets the layer visibility after a delay, which will be identical to the
  // opacity animation duration.
  layer->SetVisible(visible);
}

}  // namespace

namespace ash {
namespace internal {

ResizeShadow::ResizeShadow() : last_hit_test_(HTNOWHERE) {}

ResizeShadow::~ResizeShadow() {}

void ResizeShadow::Init(aura::Window* window) {
  // Set up our image grid and images.
  ResourceBundle& res = ResourceBundle::GetSharedInstance();
  image_grid_.reset(new ImageGrid);
  image_grid_->SetImages(
      &res.GetImageNamed(IDR_AURA_RESIZE_SHADOW_TOP_LEFT),
      &res.GetImageNamed(IDR_AURA_RESIZE_SHADOW_TOP),
      &res.GetImageNamed(IDR_AURA_RESIZE_SHADOW_TOP_RIGHT),
      &res.GetImageNamed(IDR_AURA_RESIZE_SHADOW_LEFT),
      NULL,
      &res.GetImageNamed(IDR_AURA_RESIZE_SHADOW_RIGHT),
      &res.GetImageNamed(IDR_AURA_RESIZE_SHADOW_BOTTOM_LEFT),
      &res.GetImageNamed(IDR_AURA_RESIZE_SHADOW_BOTTOM),
      &res.GetImageNamed(IDR_AURA_RESIZE_SHADOW_BOTTOM_RIGHT));
  // Initialize all layers to invisible/transparent.
  InitLayer(image_grid_->top_left_layer());
  InitLayer(image_grid_->top_layer());
  InitLayer(image_grid_->top_right_layer());
  InitLayer(image_grid_->left_layer());
  InitLayer(image_grid_->right_layer());
  InitLayer(image_grid_->bottom_left_layer());
  InitLayer(image_grid_->bottom_layer());
  InitLayer(image_grid_->bottom_right_layer());
  // Add image grid as a child of the window's layer so it follows the window
  // as it moves.
  window->layer()->Add(image_grid_->layer());
}

void ResizeShadow::ShowForHitTest(int hit) {
  // Don't start animations unless something changed.
  if (hit == last_hit_test_)
    return;
  last_hit_test_ = hit;

  // Show affected corners.
  ShowLayer(image_grid_->top_left_layer(), hit == HTTOPLEFT);
  ShowLayer(image_grid_->top_right_layer(), hit == HTTOPRIGHT);
  ShowLayer(image_grid_->bottom_left_layer(), hit == HTBOTTOMLEFT);
  ShowLayer(image_grid_->bottom_right_layer(), hit == HTBOTTOMRIGHT);

  // Show affected edges.
  ShowLayer(image_grid_->top_layer(),
            hit == HTTOPLEFT || hit == HTTOP || hit == HTTOPRIGHT);
  ShowLayer(image_grid_->left_layer(),
            hit == HTTOPLEFT || hit == HTLEFT || hit == HTBOTTOMLEFT);
  ShowLayer(image_grid_->right_layer(),
            hit == HTTOPRIGHT || hit == HTRIGHT || hit == HTBOTTOMRIGHT);
  ShowLayer(image_grid_->bottom_layer(),
            hit == HTBOTTOMLEFT || hit == HTBOTTOM || hit == HTBOTTOMRIGHT);
}

void ResizeShadow::Hide() {
  ShowForHitTest(HTNOWHERE);
}

void ResizeShadow::Layout(const gfx::Rect& content_bounds) {
  gfx::Rect local_bounds(content_bounds.size());
  image_grid_->SetContentBounds(local_bounds);
}

}  // namespace internal
}  // namespace ash
