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
      ui_thread_weak_ptr_(weak_factory_on_ui_thread_.GetWeakPtr()),
      compositor_(NULL),
      memory_policy_dirty_(false),
      hardware_allowed_(false),
      hardware_initialized_(false) {
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
  if (!client_on_ui_->RequestDrawGL(NULL, false)) {
    LOG(ERROR) << "Failed to request GL process. Deadlock likely";
  }
}

void SharedRendererState::SetCompositorOnUiThread(
    content::SynchronousCompositor* compositor) {
  base::AutoLock lock(lock_);
  DCHECK(ui_loop_->BelongsToCurrentThread());
  compositor_ = compositor;
}

content::SynchronousCompositor* SharedRendererState::GetCompositor() {
  base::AutoLock lock(lock_);
  DCHECK(compositor_);
  return compositor_;
}

void SharedRendererState::SetMemoryPolicy(
    const content::SynchronousCompositorMemoryPolicy new_policy) {
  base::AutoLock lock(lock_);
  if (memory_policy_ != new_policy) {
    memory_policy_ = new_policy;
    memory_policy_dirty_ = true;
  }
}

content::SynchronousCompositorMemoryPolicy
SharedRendererState::GetMemoryPolicy() const {
  base::AutoLock lock(lock_);
  return memory_policy_;
}

void SharedRendererState::SetDrawGLInput(const DrawGLInput& input) {
  base::AutoLock lock(lock_);
  draw_gl_input_ = input;
}

DrawGLInput SharedRendererState::GetDrawGLInput() const {
  base::AutoLock lock(lock_);
  return draw_gl_input_;
}

void SharedRendererState::SetHardwareAllowed(bool allowed) {
  base::AutoLock lock(lock_);
  hardware_allowed_ = allowed;
}

bool SharedRendererState::IsHardwareAllowed() const {
  base::AutoLock lock(lock_);
  return hardware_allowed_;
}

void SharedRendererState::SetHardwareInitialized(bool initialized) {
  base::AutoLock lock(lock_);
  hardware_initialized_ = initialized;
}

bool SharedRendererState::IsHardwareInitialized() const {
  base::AutoLock lock(lock_);
  return hardware_initialized_;
}

void SharedRendererState::SetMemoryPolicyDirty(bool is_dirty) {
  base::AutoLock lock(lock_);
  memory_policy_dirty_ = is_dirty;
}

bool SharedRendererState::IsMemoryPolicyDirty() const {
  base::AutoLock lock(lock_);
  return memory_policy_dirty_;
}

}  // namespace android_webview
