// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_compositor.h"

#include "cc/layers/layer.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

namespace vr_shell {

VrCompositor::VrCompositor(ui::WindowAndroid* window) {
  compositor_.reset(content::Compositor::Create(this, window));
}

VrCompositor::~VrCompositor() {
  if (layer_parent_ && layer_) {
    layer_parent_->AddChild(layer_);
  }
}

void VrCompositor::UpdateLayerTreeHost() {}

void VrCompositor::OnSwapBuffersCompleted(int pending_swap_buffers) {}

void VrCompositor::SetLayer(content::ContentViewCore* core) {
  assert(layer_ == nullptr);
  ui::ViewAndroid* view_android = core->GetWebContents()->GetNativeView();
  // When we pass the layer for the ContentViewCore to the compositor it may be
  // removing it from its previous parent, so we remember that and restore it to
  // its previous parent on teardown.
  layer_ = view_android->GetLayer();
  layer_parent_ = layer_->parent();
  compositor_->SetRootLayer(view_android->GetLayer());
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
