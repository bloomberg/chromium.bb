// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/webgpu_implementation.h"

#include <vector>

#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"

#define GPU_CLIENT_SINGLE_THREAD_CHECK()

namespace gpu {
namespace webgpu {

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_implementation_impl_autogen.h"

WebGPUImplementation::WebGPUImplementation(
    WebGPUCmdHelper* helper,
    TransferBufferInterface* transfer_buffer,
    GpuControl* gpu_control)
    : ImplementationBase(helper, transfer_buffer, gpu_control),
      helper_(helper),
#if BUILDFLAG(USE_DAWN)
      wire_client_(new dawn_wire::WireClient(this)),
      procs_(wire_client_->GetProcs()),
#endif
      c2s_buffer_(helper, transfer_buffer) {
}

WebGPUImplementation::~WebGPUImplementation() {}

gpu::ContextResult WebGPUImplementation::Initialize(
    const SharedMemoryLimits& limits) {
  TRACE_EVENT0("gpu", "WebGPUImplementation::Initialize");
  auto result = ImplementationBase::Initialize(limits);
  if (result != gpu::ContextResult::kSuccess) {
    return result;
  }

  // TODO(enga): Keep track of how much command space the application is using
  // and adjust c2s_buffer_size_ accordingly.
  c2s_buffer_size_ = limits.start_transfer_buffer_size;
  DCHECK_GT(c2s_buffer_size_, 0u);
  DCHECK(
      base::CheckAdd(c2s_buffer_size_, c2s_buffer_size_).IsValid<uint32_t>());

  return gpu::ContextResult::kSuccess;
}

// ContextSupport implementation.
void WebGPUImplementation::SetAggressivelyFreeResources(
    bool aggressively_free_resources) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::Swap(uint32_t flags,
                                SwapCompletedCallback complete_callback,
                                PresentationCallback presentation_callback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::SwapWithBounds(
    const std::vector<gfx::Rect>& rects,
    uint32_t flags,
    SwapCompletedCallback swap_completed,
    PresentationCallback presentation_callback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::PartialSwapBuffers(
    const gfx::Rect& sub_buffer,
    uint32_t flags,
    SwapCompletedCallback swap_completed,
    PresentationCallback presentation_callback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::CommitOverlayPlanes(
    uint32_t flags,
    SwapCompletedCallback swap_completed,
    PresentationCallback presentation_callback) {
  NOTREACHED();
}
void WebGPUImplementation::ScheduleOverlayPlane(
    int plane_z_order,
    gfx::OverlayTransform plane_transform,
    unsigned overlay_texture_id,
    const gfx::Rect& display_bounds,
    const gfx::RectF& uv_rect,
    bool enable_blend,
    unsigned gpu_fence_id) {
  NOTREACHED();
}
uint64_t WebGPUImplementation::ShareGroupTracingGUID() const {
  NOTIMPLEMENTED();
  return 0;
}
void WebGPUImplementation::SetErrorMessageCallback(
    base::RepeatingCallback<void(const char*, int32_t)> callback) {
  NOTIMPLEMENTED();
}
bool WebGPUImplementation::ThreadSafeShallowLockDiscardableTexture(
    uint32_t texture_id) {
  NOTREACHED();
  return false;
}
void WebGPUImplementation::CompleteLockDiscardableTexureOnContextThread(
    uint32_t texture_id) {
  NOTREACHED();
}
bool WebGPUImplementation::ThreadsafeDiscardableTextureIsDeletedForTracing(
    uint32_t texture_id) {
  NOTREACHED();
  return false;
}
void* WebGPUImplementation::MapTransferCacheEntry(uint32_t serialized_size) {
  NOTREACHED();
  return nullptr;
}
void WebGPUImplementation::UnmapAndCreateTransferCacheEntry(uint32_t type,
                                                            uint32_t id) {
  NOTREACHED();
}
bool WebGPUImplementation::ThreadsafeLockTransferCacheEntry(uint32_t type,
                                                            uint32_t id) {
  NOTREACHED();
  return false;
}
void WebGPUImplementation::UnlockTransferCacheEntries(
    const std::vector<std::pair<uint32_t, uint32_t>>& entries) {
  NOTREACHED();
}
void WebGPUImplementation::DeleteTransferCacheEntry(uint32_t type,
                                                    uint32_t id) {
  NOTREACHED();
}
unsigned int WebGPUImplementation::GetTransferBufferFreeSize() const {
  NOTREACHED();
  return 0;
}

// ImplementationBase implementation.
void WebGPUImplementation::IssueShallowFlush() {
  NOTIMPLEMENTED();
}

// GpuControlClient implementation.
void WebGPUImplementation::OnGpuControlLostContext() {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlLostContextMaybeReentrant() {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlErrorMessage(const char* message,
                                                    int32_t id) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlSwapBuffersCompleted(
    const SwapBuffersCompleteParams& params) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnSwapBufferPresented(
    uint64_t swap_id,
    const gfx::PresentationFeedback& feedback) {
  NOTIMPLEMENTED();
}
void WebGPUImplementation::OnGpuControlReturnData(
    base::span<const uint8_t> data) {
#if BUILDFLAG(USE_DAWN)
  wire_client_->HandleCommands(reinterpret_cast<const char*>(data.data()),
                               data.size());
#endif
}

void WebGPUImplementation::Dummy() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] wgDummy()");
  helper_->Dummy();
  helper_->Flush();
}

void* WebGPUImplementation::GetCmdSpace(size_t size) {
  // The buffer size must be initialized before any commands are serialized.
  if (c2s_buffer_size_ == 0u) {
    NOTREACHED();
    return nullptr;
  }

  // TODO(enga): Handle chunking commands if size > c2s_buffer_size_.
  if (size > c2s_buffer_size_) {
    NOTREACHED();
    return nullptr;
  }

  // This should never be more than 2 * c2s_buffer_size_ which is checked in
  // WebGPUImplementation::Initialize.
  DCHECK_LE(c2s_put_offset_, c2s_buffer_size_);
  DCHECK_LE(size, c2s_buffer_size_);
  uint32_t next_offset = c2s_put_offset_ + static_cast<uint32_t>(size);

  // If the buffer does not have enough space, or if the buffer is not
  // initialized, flush and reset the command stream.
  if (next_offset > c2s_buffer_.size() || !c2s_buffer_.valid()) {
    Flush();

    c2s_buffer_.Reset(c2s_buffer_size_);
    c2s_put_offset_ = 0;
    next_offset = size;

    if (size > c2s_buffer_.size() || !c2s_buffer_.valid()) {
      // TODO(enga): Handle OOM.
      return nullptr;
    }
  }

  DCHECK(c2s_buffer_.valid());
  uint8_t* ptr = static_cast<uint8_t*>(c2s_buffer_.address());
  ptr += c2s_put_offset_;

  c2s_put_offset_ = next_offset;
  return ptr;
}

bool WebGPUImplementation::Flush() {
  if (c2s_buffer_.valid()) {
    helper_->DawnCommands(c2s_buffer_.shm_id(), c2s_buffer_.offset(),
                          c2s_put_offset_);
    c2s_put_offset_ = 0;
    c2s_buffer_.Release();
  }
  return true;
}

}  // namespace webgpu
}  // namespace gpu
