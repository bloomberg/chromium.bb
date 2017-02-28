// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_compositor.h"

#include <utility>

#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/android/window_android.h"

namespace vr_shell {

VrCompositor::VrCompositor(ui::WindowAndroid* window, bool transparent)
    : background_color_(SK_ColorWHITE), transparent_(transparent) {
  compositor_.reset(content::Compositor::Create(this, window));
  compositor_->SetHasTransparentBackground(transparent);
}

VrCompositor::~VrCompositor() {
  RestoreLayer();
}

void VrCompositor::SetLayer(content::WebContents* web_contents) {
  RestoreLayer();
  if (!web_contents) {
    scoped_refptr<cc::SolidColorLayer> layer = cc::SolidColorLayer::Create();
    layer->SetBackgroundColor(SK_ColorTRANSPARENT);
    compositor_->SetRootLayer(std::move(layer));
    return;
  }
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

void VrCompositor::RestoreLayer() {
  if (!layer_)
    return;
  layer_->SetBackgroundColor(background_color_);
  if (layer_parent_) {
    layer_parent_->AddChild(layer_);
  }
  layer_ = nullptr;
}

void VrCompositor::SurfaceDestroyed() {
  compositor_->SetSurface(nullptr);
}

void VrCompositor::SetWindowBounds(gfx::Size size) {
  compositor_->SetWindowBounds(size);
}

void VrCompositor::SurfaceChanged(jobject surface) {
  compositor_->SetSurface(surface);
}

}  // namespace vr_shell
