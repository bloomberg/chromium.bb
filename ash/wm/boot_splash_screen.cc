// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/boot_splash_screen.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"

namespace ash {
namespace internal {

// ui::LayerDelegate that copies the aura host window's content to a ui::Layer.
class BootSplashScreen::CopyHostContentLayerDelegate
    : public ui::LayerDelegate {
 public:
  explicit CopyHostContentLayerDelegate(aura::RootWindow* root_window)
      : root_window_(root_window) {
  }

  virtual ~CopyHostContentLayerDelegate() {}

  // ui::LayerDelegate overrides:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    // It'd be safer to copy the area to a canvas in the constructor and then
    // copy from that canvas to this one here, but this appears to work (i.e. we
    // only call this before we draw our first frame) and it saves us an extra
    // copy.
    // TODO(derat): Instead of copying the data, use GLX_EXT_texture_from_pixmap
    // to create a zero-copy texture (when possible):
    // https://codereview.chromium.org/10543125
    root_window_->CopyAreaToSkCanvas(
        gfx::Rect(root_window_->GetHostOrigin(), root_window_->GetHostSize()),
        gfx::Point(), canvas->sk_canvas());
  }

  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {}

  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Closure();
  }

 private:
  aura::RootWindow* root_window_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(CopyHostContentLayerDelegate);
};

BootSplashScreen::BootSplashScreen(aura::RootWindow* root_window)
    : layer_delegate_(new CopyHostContentLayerDelegate(root_window)),
      layer_(new ui::Layer(ui::LAYER_TEXTURED)) {
  layer_->set_delegate(layer_delegate_.get());

  ui::Layer* root_layer = root_window->layer();
  layer_->SetBounds(gfx::Rect(root_layer->bounds().size()));
  root_layer->Add(layer_.get());
  root_layer->StackAtTop(layer_.get());
}

BootSplashScreen::~BootSplashScreen() {
}

void BootSplashScreen::StartHideAnimation(base::TimeDelta duration) {
  ui::ScopedLayerAnimationSettings settings(layer_->GetAnimator());
  settings.SetTransitionDuration(duration);
  settings.SetPreemptionStrategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  layer_->SetOpacity(0.0f);
}

}  // namespace internal
}  // namespace ash
