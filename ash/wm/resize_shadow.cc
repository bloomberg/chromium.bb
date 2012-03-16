// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/resize_shadow.h"

#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"

namespace {

// Height or width for the resize border shadow.
const int kShadowSize = 6;
// Amount to inset the resize effect from the window corners.
const int kShadowInset = 4;
// Color for resize effect.
const SkColor kShadowLayerColor = SkColorSetRGB(0, 0, 0);
// Final opacity for resize effect.
const int kShadowTargetOpacity = 64;
// Animation time for resize effect in milliseconds.
const int kShadowSlideDurationMs = 200;

}  // namespace

namespace ash {
namespace internal {

ResizeShadow::ResizeShadow() {}

ResizeShadow::~ResizeShadow() {}

void ResizeShadow::Init(aura::Window* window) {
  ui::Layer* parent = window->layer();
  InitLayer(parent, &top_layer_);
  InitLayer(parent, &left_layer_);
  InitLayer(parent, &right_layer_);
  InitLayer(parent, &bottom_layer_);
  InitAnimation(&top_animation_);
  InitAnimation(&left_animation_);
  InitAnimation(&right_animation_);
  InitAnimation(&bottom_animation_);
}

void ResizeShadow::ShowForHitTest(int hit_test) {
  // TODO(jamescook): Add support for top_left, top_right, bottom_left, and
  // bottom_right corners.  This approach matches the mocks but looks a little
  // funny.
  bool top = false;
  bool left = false;
  bool right = false;
  bool bottom = false;
  switch (hit_test) {
    case HTTOPLEFT:
      top = true;
      left = true;
      break;
    case HTTOP:
      top = true;
      break;
    case HTTOPRIGHT:
      top = true;
      right = true;
      break;
    case HTLEFT:
      left = true;
      break;
    case HTRIGHT:
      right = true;
      break;
    case HTBOTTOMLEFT:
      left = true;
      bottom = true;
      break;
    case HTBOTTOM:
      bottom = true;
      break;
    case HTBOTTOMRIGHT:
      bottom = true;
      right = true;
      break;
    default:
      break;
  }
  if (top)
    top_animation_->Show();
  else
    top_animation_->Hide();
  if (left)
    left_animation_->Show();
  else
    left_animation_->Hide();
  if (right)
    right_animation_->Show();
  else
    right_animation_->Hide();
  if (bottom)
    bottom_animation_->Show();
  else
    bottom_animation_->Hide();
}

void ResizeShadow::Hide() {
  // Small optimization since we frequently invoke Hide().
  if (top_animation_->IsShowing() ||
      left_animation_->IsShowing() ||
      right_animation_->IsShowing() ||
      bottom_animation_->IsShowing())
    ShowForHitTest(HTNOWHERE);
}

void ResizeShadow::Layout(const gfx::Rect& bounds) {
  // Position the resize border effects near the window edges.
  top_layer_->SetBounds(gfx::Rect(kShadowInset,
                                  -kShadowSize,
                                  bounds.width() - 2 * kShadowInset,
                                  kShadowSize));
  bottom_layer_->SetBounds(gfx::Rect(kShadowInset,
                                     bounds.height(),
                                     bounds.width() - 2 * kShadowInset,
                                     kShadowSize));
  left_layer_->SetBounds(gfx::Rect(-kShadowSize,
                                   kShadowInset,
                                   kShadowSize,
                                   bounds.height() - 2 * kShadowInset));
  right_layer_->SetBounds(gfx::Rect(bounds.width(),
                                    kShadowInset,
                                    kShadowSize,
                                    bounds.height() - 2 * kShadowInset));
}

void ResizeShadow::AnimationProgressed(const ui::Animation* animation) {
  int opacity = animation->CurrentValueBetween(0, kShadowTargetOpacity);
  if (animation == top_animation_.get())
    UpdateLayerOpacity(top_layer_.get(), opacity);
  else if (animation == left_animation_.get())
    UpdateLayerOpacity(left_layer_.get(), opacity);
  else if (animation == right_animation_.get())
    UpdateLayerOpacity(right_layer_.get(), opacity);
  else if (animation == bottom_animation_.get())
    UpdateLayerOpacity(bottom_layer_.get(), opacity);
}

void ResizeShadow::InitLayer(ui::Layer* parent,
                              scoped_ptr<ui::Layer>* new_layer) {
  // Use solid colors because they don't require video memory.
  new_layer->reset(new ui::Layer(ui::Layer::LAYER_SOLID_COLOR));
  // Transparency must be part of the color for solid color layers.
  // Start fully transparent so we can smoothly animate in.
  new_layer->get()->SetColor(SkColorSetARGB(0, 0, 0, 0));
  new_layer->get()->SetVisible(false);
  parent->Add(new_layer->get());
}

void ResizeShadow::InitAnimation(scoped_ptr<ui::SlideAnimation>* animation) {
  animation->reset(new ui::SlideAnimation(this));
  animation->get()->SetSlideDuration(kShadowSlideDurationMs);
}

void ResizeShadow::UpdateLayerOpacity(ui::Layer* layer, int opacity) {
  SkColor color = SkColorSetA(kShadowLayerColor, opacity);
  layer->SetColor(color);
  layer->SetVisible(opacity != 0);
}

}  // namespace internal
}  // namespace ash
