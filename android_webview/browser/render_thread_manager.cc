// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/render_thread_manager.h"

#include <utility>

#include "android_webview/browser/compositor_frame_producer.h"
#include "android_webview/browser/compositor_id.h"
#include "android_webview/browser/deferred_gpu_command_service.h"
#include "android_webview/browser/hardware_renderer.h"
#include "android_webview/browser/render_thread_manager_client.h"
#include "android_webview/browser/scoped_app_gl_state_restore.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace android_webview {

RenderThreadManager::RenderThreadManager(
    RenderThreadManagerClient* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_loop)
    : ui_loop_(ui_loop),
      client_(client),
      has_received_frame_(false),
      inside_hardware_release_(false),
      weak_factory_on_ui_thread_(this) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(client_);
  ui_thread_weak_ptr_ = weak_factory_on_ui_thread_.GetWeakPtr();
}

RenderThreadManager::~RenderThreadManager() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(!hardware_renderer_.get());
  DCHECK(child_frames_.empty());
}

void RenderThreadManager::UpdateParentDrawConstraintsOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  if (producer_weak_ptr_) {
    producer_weak_ptr_->OnParentDrawConstraintsUpdated(this);
  }
}

void RenderThreadManager::SetScrollOffsetOnUI(gfx::Vector2d scroll_offset) {
  base::AutoLock lock(lock_);
  scroll_offset_ = scroll_offset;
}

gfx::Vector2d RenderThreadManager::GetScrollOffsetOnRT() {
  base::AutoLock lock(lock_);
  return scroll_offset_;
}

std::unique_ptr<ChildFrame> RenderThreadManager::SetFrameOnUI(
    std::unique_ptr<ChildFrame> new_frame) {
  DCHECK(new_frame);
  has_received_frame_ = true;

  base::AutoLock lock(lock_);
  if (child_frames_.empty()) {
    child_frames_.emplace_back(std::move(new_frame));
    return nullptr;
  }
  std::unique_ptr<ChildFrame> uncommitted_frame;
  DCHECK_LE(child_frames_.size(), 2u);
  ChildFrameQueue pruned_frames =
      HardwareRenderer::WaitAndPruneFrameQueue(&child_frames_);
  DCHECK_LE(pruned_frames.size(), 1u);
  if (pruned_frames.size())
    uncommitted_frame = std::move(pruned_frames.front());
  child_frames_.emplace_back(std::move(new_frame));
  return uncommitted_frame;
}

ChildFrameQueue RenderThreadManager::PassFramesOnRT() {
  base::AutoLock lock(lock_);
  ChildFrameQueue returned_frames;
  returned_frames.swap(child_frames_);
  return returned_frames;
}

ChildFrameQueue RenderThreadManager::PassUncommittedFrameOnUI() {
  base::AutoLock lock(lock_);
  for (auto& frame_ptr : child_frames_)
    frame_ptr->WaitOnFutureIfNeeded();
  ChildFrameQueue returned_frames;
  returned_frames.swap(child_frames_);
  return returned_frames;
}

void RenderThreadManager::PostExternalDrawConstraintsToChildCompositorOnRT(
    const ParentCompositorDrawConstraints& parent_draw_constraints) {
  {
    base::AutoLock lock(lock_);
    parent_draw_constraints_ = parent_draw_constraints;
  }

  // No need to hold the lock_ during the post task.
  ui_loop_->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderThreadManager::UpdateParentDrawConstraintsOnUI,
                     ui_thread_weak_ptr_));
}

ParentCompositorDrawConstraints
RenderThreadManager::GetParentDrawConstraintsOnUI() const {
  base::AutoLock lock(lock_);
  return parent_draw_constraints_;
}

void RenderThreadManager::SetInsideHardwareRelease(bool inside) {
  base::AutoLock lock(lock_);
  inside_hardware_release_ = inside;
}

bool RenderThreadManager::IsInsideHardwareRelease() const {
  base::AutoLock lock(lock_);
  return inside_hardware_release_;
}

void RenderThreadManager::InsertReturnedResourcesOnRT(
    const std::vector<viz::ReturnedResource>& resources,
    const CompositorID& compositor_id,
    uint32_t layer_tree_frame_sink_id) {
  if (resources.empty())
    return;
  ui_loop_->PostTask(
      FROM_HERE, base::BindOnce(&CompositorFrameProducer::ReturnUsedResources,
                                producer_weak_ptr_, resources, compositor_id,
                                layer_tree_frame_sink_id));
}

void RenderThreadManager::DrawGL(AwDrawGLInfo* draw_info) {
  TRACE_EVENT0("android_webview,toplevel", "DrawFunctor");
  if (draw_info->mode == AwDrawGLInfo::kModeSync) {
    TRACE_EVENT_INSTANT0("android_webview", "kModeSync",
                         TRACE_EVENT_SCOPE_THREAD);
    if (hardware_renderer_)
      hardware_renderer_->CommitFrame();
    return;
  }

  // Force GL binding init if it's not yet initialized.
  DeferredGpuCommandService::GetInstance();

  ScopedAppGLStateRestore state_restore(
      draw_info->mode == AwDrawGLInfo::kModeDraw
          ? ScopedAppGLStateRestore::MODE_DRAW
          : ScopedAppGLStateRestore::MODE_RESOURCE_MANAGEMENT,
      draw_info->version < 3 /* save_restore */);
  ScopedAllowGL allow_gl;
  if (!hardware_renderer_ && draw_info->mode == AwDrawGLInfo::kModeDraw &&
      !IsInsideHardwareRelease() && HasFrameForHardwareRendererOnRT()) {
    hardware_renderer_.reset(new HardwareRenderer(this));
    hardware_renderer_->CommitFrame();
  }

  if (draw_info->mode == AwDrawGLInfo::kModeProcessNoContext) {
    LOG(ERROR) << "Received unexpected kModeProcessNoContext";
  }

  if (IsInsideHardwareRelease()) {
    hardware_renderer_.reset();
    return;
  }

  if (draw_info->mode != AwDrawGLInfo::kModeDraw)
    return;
  if (hardware_renderer_)
    hardware_renderer_->DrawGL(draw_info);
}

void RenderThreadManager::RemoveFromCompositorFrameProducerOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  if (producer_weak_ptr_)
    producer_weak_ptr_->RemoveCompositorFrameConsumer(this);
}

void RenderThreadManager::DeleteHardwareRendererOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());

  InsideHardwareReleaseReset auto_inside_hardware_release_reset(this);

  client_->DetachFunctorFromView();

  // If the WebView gets onTrimMemory >= MODERATE twice in a row, the 2nd
  // onTrimMemory will result in an unnecessary Render Thread InvokeGL call.
  if (has_received_frame_) {
    // Receiving at least one frame is a precondition for
    // initialization (such as looing up GL bindings and constructing
    // hardware_renderer_).
    bool draw_functor_succeeded = client_->RequestInvokeGL(true);
    if (!draw_functor_succeeded) {
      LOG(ERROR) << "Unable to free GL resources. Has the Window leaked?";
      // Calling release on wrong thread intentionally.
      AwDrawGLInfo info;
      info.mode = AwDrawGLInfo::kModeProcess;
      DrawGL(&info);
    }
  }

  has_received_frame_ = false;
}

void RenderThreadManager::SetCompositorFrameProducer(
    CompositorFrameProducer* compositor_frame_producer) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  producer_weak_ptr_ = compositor_frame_producer->GetWeakPtr();
}

bool RenderThreadManager::HasFrameForHardwareRendererOnRT() const {
  base::AutoLock lock(lock_);
  return !child_frames_.empty();
}

RenderThreadManager::InsideHardwareReleaseReset::InsideHardwareReleaseReset(
    RenderThreadManager* render_thread_manager)
    : render_thread_manager_(render_thread_manager) {
  DCHECK(!render_thread_manager_->IsInsideHardwareRelease());
  render_thread_manager_->SetInsideHardwareRelease(true);
}

RenderThreadManager::InsideHardwareReleaseReset::~InsideHardwareReleaseReset() {
  render_thread_manager_->SetInsideHardwareRelease(false);
}

}  // namespace android_webview
