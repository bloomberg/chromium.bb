// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/boot_splash_screen_chromeos.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"

#if defined(USE_X11)
#include "ui/base/x/x11_util.h"  // nogncheck
#endif

namespace ash {

// ui::LayerDelegate that copies the aura host window's content to a ui::Layer.
class BootSplashScreen::CopyHostContentLayerDelegate
    : public ui::LayerDelegate {
 public:
  explicit CopyHostContentLayerDelegate(aura::WindowTreeHost* host)
      : host_(host) {
  }

  ~CopyHostContentLayerDelegate() override {}

  // ui::LayerDelegate overrides:
  void OnPaintLayer(const ui::PaintContext& context) override {
    // It'd be safer to copy the area to a canvas in the constructor and then
    // copy from that canvas to this one here, but this appears to work (i.e. we
    // only call this before we draw our first frame) and it saves us an extra
    // copy.
    // TODO(derat): Instead of copying the data, use GLX_EXT_texture_from_pixmap
    // to create a zero-copy texture (when possible):
    // https://codereview.chromium.org/10543125
#if defined(USE_X11)
    ui::PaintRecorder recorder(context, host_->GetBounds().size());
    ui::CopyAreaToCanvas(host_->GetAcceleratedWidget(), host_->GetBounds(),
                         gfx::Point(), recorder.canvas());
#else
    // TODO(spang): Figure out what to do here.
    NOTIMPLEMENTED();
#endif
  }

  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override {}

  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}

  base::Closure PrepareForLayerBoundsChange() override {
    return base::Closure();
  }

 private:
  aura::WindowTreeHost* host_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(CopyHostContentLayerDelegate);
};

BootSplashScreen::BootSplashScreen(aura::WindowTreeHost* host)
    : layer_delegate_(new CopyHostContentLayerDelegate(host)),
      layer_(new ui::Layer(ui::LAYER_TEXTURED)) {
  layer_->set_delegate(layer_delegate_.get());

  ui::Layer* root_layer = host->window()->layer();
  layer_->SetBounds(gfx::Rect(host->window()->bounds().size()));
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

}  // namespace ash
