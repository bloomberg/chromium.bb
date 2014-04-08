// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/shared_renderer_state.h"

#include "android_webview/browser/browser_view_renderer_client.h"
#include "base/bind.h"
#include "base/location.h"

namespace android_webview {

DrawGLInput::DrawGLInput() : frame_id(0), width(0), height(0) {}

DrawGLResult::DrawGLResult() : frame_id(0), clip_contains_visible_rect(false) {}

SharedRendererState::SharedRendererState(
    scoped_refptr<base::MessageLoopProxy> ui_loop,
    BrowserViewRendererClient* client)
    : ui_loop_(ui_loop),
      client_on_ui_(client),
      weak_factory_on_ui_thread_(this),
      ui_thread_weak_ptr_(weak_factory_on_ui_thread_.GetWeakPtr()) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(client_on_ui_);
}

SharedRendererState::~SharedRendererState() {}

void SharedRendererState::ClientRequestDrawGL() {
  if (ui_loop_->BelongsToCurrentThread()) {
    ClientRequestDrawGLOnUIThread();
  } else {
    ui_loop_->PostTask(
        FROM_HERE,
        base::Bind(&SharedRendererState::ClientRequestDrawGLOnUIThread,
                   ui_thread_weak_ptr_));
  }
}

void SharedRendererState::ClientRequestDrawGLOnUIThread() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  if (!client_on_ui_->RequestDrawGL(NULL)) {
    LOG(ERROR) << "Failed to request GL process. Deadlock likely";
  }
}

void SharedRendererState::SetCompositorOnUiThread(
    content::SynchronousCompositor* compositor) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  compositor_ = compositor;
}

content::SynchronousCompositor* SharedRendererState::GetCompositor() {
  DCHECK(compositor_);
  return compositor_;
}

void SharedRendererState::SetDrawGLInput(const DrawGLInput& input) {
  draw_gl_input_ = input;
}

DrawGLInput SharedRendererState::GetDrawGLInput() const {
  return draw_gl_input_;
}

}  // namespace android_webview
