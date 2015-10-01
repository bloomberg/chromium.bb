// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/shared_renderer_state.h"

#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/child_frame.h"
#include "android_webview/browser/deferred_gpu_command_service.h"
#include "android_webview/browser/hardware_renderer.h"
#include "android_webview/browser/scoped_app_gl_state_restore.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event_argument.h"

namespace android_webview {

namespace internal {

class RequestDrawGLTracker {
 public:
  RequestDrawGLTracker();
  bool ShouldRequestOnNonUiThread(SharedRendererState* state);
  bool ShouldRequestOnUiThread(SharedRendererState* state);
  void ResetPending();
  void SetQueuedFunctorOnUi(SharedRendererState* state);

 private:
  base::Lock lock_;
  SharedRendererState* pending_ui_;
  SharedRendererState* pending_non_ui_;
};

RequestDrawGLTracker::RequestDrawGLTracker()
    : pending_ui_(NULL), pending_non_ui_(NULL) {
}

bool RequestDrawGLTracker::ShouldRequestOnNonUiThread(
    SharedRendererState* state) {
  base::AutoLock lock(lock_);
  if (pending_ui_ || pending_non_ui_)
    return false;
  pending_non_ui_ = state;
  return true;
}

bool RequestDrawGLTracker::ShouldRequestOnUiThread(SharedRendererState* state) {
  base::AutoLock lock(lock_);
  if (pending_non_ui_) {
    pending_non_ui_->ResetRequestDrawGLCallback();
    pending_non_ui_ = NULL;
  }
  // At this time, we could have already called RequestDrawGL on the UI thread,
  // but the corresponding GL mode process hasn't happened yet. In this case,
  // don't schedule another requestDrawGL on the UI thread.
  if (pending_ui_)
    return false;
  pending_ui_ = state;
  return true;
}

void RequestDrawGLTracker::ResetPending() {
  base::AutoLock lock(lock_);
  pending_non_ui_ = NULL;
  pending_ui_ = NULL;
}

void RequestDrawGLTracker::SetQueuedFunctorOnUi(SharedRendererState* state) {
  base::AutoLock lock(lock_);
  DCHECK(state);
  pending_ui_ = state;
  pending_non_ui_ = NULL;
}

}  // namespace internal

namespace {

base::LazyInstance<internal::RequestDrawGLTracker> g_request_draw_gl_tracker =
    LAZY_INSTANCE_INITIALIZER;

}

SharedRendererState::SharedRendererState(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_loop,
    BrowserViewRenderer* browser_view_renderer)
    : ui_loop_(ui_loop),
      browser_view_renderer_(browser_view_renderer),
      renderer_manager_key_(GLViewRendererManager::GetInstance()->NullKey()),
      hardware_renderer_has_frame_(false),
      inside_hardware_release_(false),
      weak_factory_on_ui_thread_(this) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(browser_view_renderer_);
  ui_thread_weak_ptr_ = weak_factory_on_ui_thread_.GetWeakPtr();
  ResetRequestDrawGLCallback();
}

SharedRendererState::~SharedRendererState() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(!hardware_renderer_.get());
}

void SharedRendererState::ClientRequestDrawGL(bool for_idle) {
  if (ui_loop_->BelongsToCurrentThread()) {
    if (!g_request_draw_gl_tracker.Get().ShouldRequestOnUiThread(this))
      return;
    ClientRequestDrawGLOnUI();
  } else {
    if (!g_request_draw_gl_tracker.Get().ShouldRequestOnNonUiThread(this))
      return;
    base::Closure callback;
    {
      base::AutoLock lock(lock_);
      callback = request_draw_gl_closure_;
    }
    // 17ms is slightly longer than a frame, hoping that it will come
    // after the next frame so that the idle work is taken care off by
    // the next frame instead.
    ui_loop_->PostDelayedTask(
        FROM_HERE, callback,
        for_idle ? base::TimeDelta::FromMilliseconds(17) : base::TimeDelta());
  }
}

void SharedRendererState::DidDrawGLProcess() {
  g_request_draw_gl_tracker.Get().ResetPending();
}

void SharedRendererState::ResetRequestDrawGLCallback() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  request_draw_gl_cancelable_closure_.Reset(base::Bind(
      &SharedRendererState::ClientRequestDrawGLOnUI, base::Unretained(this)));
  request_draw_gl_closure_ = request_draw_gl_cancelable_closure_.callback();
}

void SharedRendererState::ClientRequestDrawGLOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  ResetRequestDrawGLCallback();
  g_request_draw_gl_tracker.Get().SetQueuedFunctorOnUi(this);
  if (!browser_view_renderer_->RequestDrawGL(false)) {
    g_request_draw_gl_tracker.Get().ResetPending();
    LOG(ERROR) << "Failed to request GL process. Deadlock likely";
  }
}

void SharedRendererState::UpdateParentDrawConstraintsOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  browser_view_renderer_->UpdateParentDrawConstraints();
}

void SharedRendererState::SetScrollOffsetOnUI(gfx::Vector2d scroll_offset) {
  base::AutoLock lock(lock_);
  scroll_offset_ = scroll_offset;
}

gfx::Vector2d SharedRendererState::GetScrollOffsetOnRT() {
  base::AutoLock lock(lock_);
  return scroll_offset_;
}

void SharedRendererState::SetCompositorFrameOnUI(scoped_ptr<ChildFrame> frame) {
  base::AutoLock lock(lock_);
  DCHECK(!child_frame_.get());
  child_frame_ = frame.Pass();
}

scoped_ptr<ChildFrame> SharedRendererState::PassCompositorFrameOnRT() {
  base::AutoLock lock(lock_);
  hardware_renderer_has_frame_ =
      hardware_renderer_has_frame_ || child_frame_.get();
  return child_frame_.Pass();
}

scoped_ptr<ChildFrame> SharedRendererState::PassUncommittedFrameOnUI() {
  base::AutoLock lock(lock_);
  return child_frame_.Pass();
}

void SharedRendererState::PostExternalDrawConstraintsToChildCompositorOnRT(
    const ParentCompositorDrawConstraints& parent_draw_constraints) {
  {
    base::AutoLock lock(lock_);
    parent_draw_constraints_ = parent_draw_constraints;
  }

  // No need to hold the lock_ during the post task.
  ui_loop_->PostTask(
      FROM_HERE,
      base::Bind(&SharedRendererState::UpdateParentDrawConstraintsOnUI,
                 ui_thread_weak_ptr_));
}

ParentCompositorDrawConstraints
SharedRendererState::GetParentDrawConstraintsOnUI() const {
  base::AutoLock lock(lock_);
  return parent_draw_constraints_;
}

void SharedRendererState::SetInsideHardwareRelease(bool inside) {
  base::AutoLock lock(lock_);
  inside_hardware_release_ = inside;
}

bool SharedRendererState::IsInsideHardwareRelease() const {
  base::AutoLock lock(lock_);
  return inside_hardware_release_;
}

void SharedRendererState::InsertReturnedResourcesOnRT(
    const cc::ReturnedResourceArray& resources) {
  base::AutoLock lock(lock_);
  returned_resources_.insert(
      returned_resources_.end(), resources.begin(), resources.end());
}

void SharedRendererState::SwapReturnedResourcesOnUI(
    cc::ReturnedResourceArray* resources) {
  DCHECK(resources->empty());
  base::AutoLock lock(lock_);
  resources->swap(returned_resources_);
}

bool SharedRendererState::ReturnedResourcesEmptyOnUI() const {
  base::AutoLock lock(lock_);
  return returned_resources_.empty();
}

void SharedRendererState::DrawGL(AwDrawGLInfo* draw_info) {
  TRACE_EVENT0("android_webview", "DrawFunctor");
  if (draw_info->mode == AwDrawGLInfo::kModeSync) {
    TRACE_EVENT_INSTANT0("android_webview", "kModeSync",
                         TRACE_EVENT_SCOPE_THREAD);
    if (hardware_renderer_)
      hardware_renderer_->CommitFrame();
    return;
  }

  // kModeProcessNoContext should never happen because we tear down hardware
  // in onTrimMemory. However that guarantee is maintained outside of chromium
  // code. Not notifying shared state in kModeProcessNoContext can lead to
  // immediate deadlock, which is slightly more catastrophic than leaks or
  // corruption.
  if (draw_info->mode == AwDrawGLInfo::kModeProcess ||
      draw_info->mode == AwDrawGLInfo::kModeProcessNoContext) {
    DidDrawGLProcess();
  }

  {
    GLViewRendererManager* manager = GLViewRendererManager::GetInstance();
    base::AutoLock lock(lock_);
    if (renderer_manager_key_ != manager->NullKey()) {
      manager->DidDrawGL(renderer_manager_key_);
    }
  }

  ScopedAppGLStateRestore state_restore(
      draw_info->mode == AwDrawGLInfo::kModeDraw
          ? ScopedAppGLStateRestore::MODE_DRAW
          : ScopedAppGLStateRestore::MODE_RESOURCE_MANAGEMENT);
  // Set the correct FBO before kModeDraw. The GL commands run in kModeDraw
  // require a correctly bound FBO. The FBO remains until the next kModeDraw.
  // So kModeProcess between kModeDraws has correctly bound FBO, too.
  if (draw_info->mode == AwDrawGLInfo::kModeDraw) {
    if (!hardware_renderer_) {
      hardware_renderer_.reset(new HardwareRenderer(this));
      hardware_renderer_->CommitFrame();
    }
    hardware_renderer_->SetBackingFrameBufferObject(
        state_restore.framebuffer_binding_ext());
  }

  ScopedAllowGL allow_gl;

  if (draw_info->mode == AwDrawGLInfo::kModeProcessNoContext) {
    LOG(ERROR) << "Received unexpected kModeProcessNoContext";
  }

  if (IsInsideHardwareRelease()) {
    hardware_renderer_has_frame_ = false;
    hardware_renderer_.reset();
    // Flush the idle queue in tear down.
    DeferredGpuCommandService::GetInstance()->PerformAllIdleWork();
    return;
  }

  if (draw_info->mode != AwDrawGLInfo::kModeDraw) {
    if (draw_info->mode == AwDrawGLInfo::kModeProcess) {
      DeferredGpuCommandService::GetInstance()->PerformIdleWork(true);
    }
    return;
  }

  hardware_renderer_->DrawGL(state_restore.stencil_enabled(), draw_info);
  DeferredGpuCommandService::GetInstance()->PerformIdleWork(false);
}

void SharedRendererState::ReleaseHardwareDrawIfNeededOnUI() {
  ReleaseCompositorResourcesIfNeededOnUI(true);
}

void SharedRendererState::DeleteHardwareRendererOnUI() {
  ReleaseCompositorResourcesIfNeededOnUI(false);
}

void SharedRendererState::ReleaseCompositorResourcesIfNeededOnUI(
    bool release_hardware_draw) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  InsideHardwareReleaseReset auto_inside_hardware_release_reset(this);

  browser_view_renderer_->DetachFunctorFromView();
  bool hardware_initialized = browser_view_renderer_->hardware_enabled();
  // If the WebView gets onTrimMemory >= MODERATE twice in a row, the 2nd
  // onTrimMemory will result in an unnecessary Render Thread DrawGL call.
  if (hardware_initialized) {
    bool draw_functor_succeeded = browser_view_renderer_->RequestDrawGL(true);
    if (!draw_functor_succeeded) {
      LOG(ERROR) << "Unable to free GL resources. Has the Window leaked?";
      // Calling release on wrong thread intentionally.
      AwDrawGLInfo info;
      info.mode = AwDrawGLInfo::kModeProcess;
      DrawGL(&info);
    }

    if (release_hardware_draw)
      browser_view_renderer_->ReleaseHardware();
  }

  GLViewRendererManager* manager = GLViewRendererManager::GetInstance();

  {
    base::AutoLock lock(lock_);
    if (renderer_manager_key_ != manager->NullKey()) {
      manager->Remove(renderer_manager_key_);
      renderer_manager_key_ = manager->NullKey();
    }
  }

  if (hardware_initialized) {
    // Flush any invoke functors that's caused by ReleaseHardware.
    browser_view_renderer_->RequestDrawGL(true);
  }
}

bool SharedRendererState::HasFrameOnUI() const {
  base::AutoLock lock(lock_);
  return hardware_renderer_has_frame_ || child_frame_.get();
}

void SharedRendererState::InitializeHardwareDrawIfNeededOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  GLViewRendererManager* manager = GLViewRendererManager::GetInstance();

  base::AutoLock lock(lock_);
  if (renderer_manager_key_ == manager->NullKey()) {
    renderer_manager_key_ = manager->PushBack(this);
  }
}

SharedRendererState::InsideHardwareReleaseReset::InsideHardwareReleaseReset(
    SharedRendererState* shared_renderer_state)
    : shared_renderer_state_(shared_renderer_state) {
  DCHECK(!shared_renderer_state_->IsInsideHardwareRelease());
  shared_renderer_state_->SetInsideHardwareRelease(true);
}

SharedRendererState::InsideHardwareReleaseReset::~InsideHardwareReleaseReset() {
  shared_renderer_state_->SetInsideHardwareRelease(false);
}

}  // namespace android_webview
