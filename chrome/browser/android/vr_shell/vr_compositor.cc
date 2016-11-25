// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_compositor.h"

#include "cc/layers/layer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/window_android.h"

namespace vr_shell {

VrCompositor::VrCompositor(ui::WindowAndroid* window, bool transparent)
    : background_color_(SK_ColorWHITE),
      transparent_(transparent) {
  compositor_.reset(content::Compositor::Create(this, window));
  compositor_->SetHasTransparentBackground(transparent);
}

VrCompositor::~VrCompositor() {
  if (layer_) {
    layer_->SetBackgroundColor(background_color_);
    if (layer_parent_) {
      layer_parent_->AddChild(layer_);
    }
  }
}

void VrCompositor::UpdateLayerTreeHost() {}

void VrCompositor::OnSwapBuffersCompleted(int pending_swap_buffers) {}

void VrCompositor::SetLayer(content::WebContents* web_contents) {
  assert(layer_ == nullptr);
  ui::ViewAndroid* view_android = web_contents->GetNativeView();
  // When we pass the layer for the ContentViewCore to the compositor it may be
  // removing it from its previous parent, so we remember that and restore it to
  // its previous parent on teardown.
  layer_ = view_android->GetLayer();

  // Remember the old background color to be restored later.
  background_color_ = layer_->background_color();
  if (transparent_) {
    layer_->SetBackgroundColor(SK_ColorTRANSPARENT);
  }
  layer_parent_ = layer_->parent();
  compositor_->SetRootLayer(layer_);
}

void VrCompositor::SurfaceDestroyed() {
  compositor_->SetSurface(nullptr);
}

void VrCompositor::SurfaceChanged(
    int width,
    int height,
    const base::android::JavaParamRef<jobject>& surface) {
  DCHECK(surface);
  compositor_->SetSurface(surface);
  compositor_->SetWindowBounds(gfx::Size(width, height));
}

}  // namespace vr_shell
