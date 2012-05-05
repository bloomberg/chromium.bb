// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/screen_dimmer.h"

#include "ash/shell.h"
#include "base/time.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace ash {
namespace internal {

namespace {

// Opacity for |dimming_layer_| when it's dimming the screen.
const float kDimmingLayerOpacity = 0.4f;

// Duration for dimming animations, in milliseconds.
const int kDimmingTransitionMs = 200;

}  // namespace

ScreenDimmer::ScreenDimmer() : currently_dimming_(false) {
  Shell::GetInstance()->GetRootWindow()->AddRootWindowObserver(this);
}

ScreenDimmer::~ScreenDimmer() {
  Shell::GetInstance()->GetRootWindow()->RemoveRootWindowObserver(this);
}

void ScreenDimmer::SetDimming(bool should_dim) {
  if (should_dim == currently_dimming_)
    return;

  if (!dimming_layer_.get()) {
    dimming_layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    dimming_layer_->SetColor(SK_ColorBLACK);
    dimming_layer_->SetOpacity(0.0f);
    ui::Layer* root_layer = Shell::GetRootWindow()->layer();
    dimming_layer_->SetBounds(root_layer->bounds());
    root_layer->Add(dimming_layer_.get());
    root_layer->StackAtTop(dimming_layer_.get());
  }

  currently_dimming_ = should_dim;

  ui::ScopedLayerAnimationSettings scoped_settings(
      dimming_layer_->GetAnimator());
  scoped_settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kDimmingTransitionMs));
  dimming_layer_->SetOpacity(should_dim ? kDimmingLayerOpacity : 0.0f);
}

void ScreenDimmer::OnRootWindowResized(const aura::RootWindow* root,
                                       const gfx::Size& old_size) {
  if (dimming_layer_.get())
    dimming_layer_->SetBounds(gfx::Rect(root->bounds().size()));
}

}  // namespace internal
}  // namespace ash
