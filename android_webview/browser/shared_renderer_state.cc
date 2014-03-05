// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/shared_renderer_state.h"

#include "android_webview/browser/browser_view_renderer_client.h"
#include "base/bind.h"
#include "base/location.h"

using base::AutoLock;

namespace android_webview {

DrawGLInput::DrawGLInput() : frame_id(0) {}

DrawGLResult::DrawGLResult() : frame_id(0), clip_contains_visible_rect(false) {}

namespace internal {

BothThreads::BothThreads() : compositor(NULL) {}

}  // namespace internal

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
  AutoLock lock(lock_);
  DCHECK(ui_loop_->BelongsToCurrentThread());
  both().compositor = compositor;
}

bool SharedRendererState::CompositorInitializeHwDraw(
    scoped_refptr<gfx::GLSurface> surface) {
  AutoLock lock(lock_);
  DCHECK(both().compositor);
  return both().compositor->InitializeHwDraw(surface);
}

void SharedRendererState::CompositorReleaseHwDraw() {
  AutoLock lock(lock_);
  DCHECK(both().compositor);
  both().compositor->ReleaseHwDraw();
}

bool SharedRendererState::CompositorDemandDrawHw(
    gfx::Size surface_size,
    const gfx::Transform& transform,
    gfx::Rect viewport,
    gfx::Rect clip,
    bool stencil_enabled) {
  AutoLock lock(lock_);
  DCHECK(both().compositor);
  return both().compositor->DemandDrawHw(
      surface_size, transform, viewport, clip, stencil_enabled);
}

bool SharedRendererState::CompositorDemandDrawSw(SkCanvas* canvas) {
  AutoLock lock(lock_);
  DCHECK(both().compositor);
  return both().compositor->DemandDrawSw(canvas);
}

void SharedRendererState::CompositorSetMemoryPolicy(
    const content::SynchronousCompositorMemoryPolicy& policy) {
  AutoLock lock(lock_);
  DCHECK(both().compositor);
  return both().compositor->SetMemoryPolicy(policy);
}

void SharedRendererState::CompositorDidChangeRootLayerScrollOffset() {
  AutoLock lock(lock_);
  DCHECK(both().compositor);
  both().compositor->DidChangeRootLayerScrollOffset();
}

void SharedRendererState::SetDrawGLInput(const DrawGLInput& input) {
  AutoLock lock(lock_);
  both().draw_gl_input = input;
}

DrawGLInput SharedRendererState::GetDrawGLInput() const {
  AutoLock lock(lock_);
  return both().draw_gl_input;
}

internal::BothThreads& SharedRendererState::both() {
  lock_.AssertAcquired();
  return both_threads_;
}

const internal::BothThreads& SharedRendererState::both() const {
  lock_.AssertAcquired();
  return both_threads_;
}

}  // namespace android_webview
