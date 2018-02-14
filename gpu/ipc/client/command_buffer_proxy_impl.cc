// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/command_buffer_proxy_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/common/gpu_param_traits.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_switches_util.h"

namespace gpu {

namespace {

gpu::CommandBufferId CommandBufferProxyID(int channel_id, int32_t route_id) {
  return gpu::CommandBufferId::FromUnsafeValue(
      (static_cast<uint64_t>(channel_id) << 32) | route_id);
}

int GetChannelID(gpu::CommandBufferId command_buffer_id) {
  return static_cast<int>(command_buffer_id.GetUnsafeValue() >> 32);
}

}  // namespace

CommandBufferProxyImpl::CommandBufferProxyImpl(
    scoped_refptr<GpuChannelHost> channel,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    int32_t stream_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : channel_(std::move(channel)),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      channel_id_(channel_->channel_id()),
      route_id_(channel_->GenerateRouteID()),
      stream_id_(stream_id),
      command_buffer_id_(CommandBufferProxyID(channel_id_, route_id_)),
      callback_thread_(std::move(task_runner)),
      weak_ptr_factory_(this) {
  DCHECK(route_id_);
}

CommandBufferProxyImpl::~CommandBufferProxyImpl() {
  for (auto& observer : deletion_observers_)
    observer.OnWillDeleteImpl();
  DisconnectChannel();
}

ContextResult CommandBufferProxyImpl::Initialize(
    gpu::SurfaceHandle surface_handle,
    CommandBufferProxyImpl* share_group,
    gpu::SchedulingPriority stream_priority,
    const gpu::ContextCreationAttribs& attribs,
    const GURL& active_url) {
  DCHECK(!share_group || (stream_id_ == share_group->stream_id_));
  TRACE_EVENT1("gpu", "GpuChannelHost::CreateViewCommandBuffer",
               "surface_handle", surface_handle);

  // Drop the |channel_| if this method does not succeed and early-outs, to
  // prevent cleanup on destruction.
  auto channel = std::move(channel_);

  GPUCreateCommandBufferConfig init_params;
  init_params.surface_handle = surface_handle;
  init_params.share_group_id =
      share_group ? share_group->route_id_ : MSG_ROUTING_NONE;
  init_params.stream_id = stream_id_;
  init_params.stream_priority = stream_priority;
  init_params.attribs = attribs;
  init_params.active_url = active_url;

  TRACE_EVENT0("gpu", "CommandBufferProxyImpl::Initialize");
  shared_state_shm_ = AllocateAndMapSharedMemory(sizeof(*shared_state()));
  if (!shared_state_shm_) {
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "AllocateAndMapSharedMemory failed";
    return ContextResult::kFatalFailure;
  }

  shared_state()->Initialize();

  // This handle is owned by the GPU process and must be passed to it or it
  // will leak. In otherwords, do not early out on error between here and the
  // sending of the CreateCommandBuffer IPC below.
  base::SharedMemoryHandle handle =
      channel->ShareToGpuProcess(shared_state_shm_->handle());
  if (!base::SharedMemory::IsHandleValid(handle)) {
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "Shared memory handle is not valid";
    return ContextResult::kFatalFailure;
  }

  // Route must be added before sending the message, otherwise messages sent
  // from the GPU process could race against adding ourselves to the filter.
  channel->AddRouteWithTaskRunner(route_id_, weak_ptr_factory_.GetWeakPtr(),
                                  callback_thread_);

  // We're blocking the UI thread, which is generally undesirable.
  // In this case we need to wait for this before we can show any UI /anyway/,
  // so it won't cause additional jank.
  // TODO(piman): Make this asynchronous (http://crbug.com/125248).
  ContextResult result = ContextResult::kSuccess;
  bool sent = channel->Send(new GpuChannelMsg_CreateCommandBuffer(
      init_params, route_id_, handle, &result, &capabilities_));
  if (!sent) {
    channel->RemoveRoute(route_id_);
    LOG(ERROR) << "ContextResult::kTransientFailure: "
                  "Failed to send GpuChannelMsg_CreateCommandBuffer.";
    return ContextResult::kTransientFailure;
  }
  if (result != ContextResult::kSuccess) {
    DLOG(ERROR) << "Failure processing GpuChannelMsg_CreateCommandBuffer.";
    channel->RemoveRoute(route_id_);
    return result;
  }

  channel_ = std::move(channel);
  return result;
}

bool CommandBufferProxyImpl::OnMessageReceived(const IPC::Message& message) {
  base::Optional<base::AutoLock> lock;
  if (lock_)
    lock.emplace(*lock_);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CommandBufferProxyImpl, message)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Destroyed, OnDestroyed);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_ConsoleMsg, OnConsoleMessage);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalAck, OnSignalAck);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SwapBuffersCompleted,
                        OnSwapBuffersCompleted);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_UpdateVSyncParameters,
                        OnUpdateVSyncParameters);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_BufferPresented, OnBufferPresented);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_GetGpuFenceHandleComplete,
                        OnGetGpuFenceHandleComplete);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  if (!handled) {
    LOG(ERROR) << "Gpu process sent invalid message.";
    base::AutoLock last_state_lock(last_state_lock_);
    OnGpuAsyncMessageError(gpu::error::kInvalidGpuMessage,
                           gpu::error::kLostContext);
  }
  return handled;
}

void CommandBufferProxyImpl::OnChannelError() {
  base::Optional<base::AutoLock> lock;
  if (lock_)
    lock.emplace(*lock_);
  base::AutoLock last_state_lock(last_state_lock_);

  gpu::error::ContextLostReason context_lost_reason =
      gpu::error::kGpuChannelLost;
  if (shared_state_shm_ && shared_state_shm_->memory()) {
    // The GPU process might have intentionally been crashed
    // (exit_on_context_lost), so try to find out the original reason.
    TryUpdateStateDontReportError();
    if (last_state_.error == gpu::error::kLostContext)
      context_lost_reason = last_state_.context_lost_reason;
  }
  OnGpuAsyncMessageError(context_lost_reason, gpu::error::kLostContext);
}

void CommandBufferProxyImpl::OnDestroyed(gpu::error::ContextLostReason reason,
                                         gpu::error::Error error) {
  base::AutoLock lock(last_state_lock_);
  OnGpuAsyncMessageError(reason, error);
}

void CommandBufferProxyImpl::OnConsoleMessage(
    const GPUCommandBufferConsoleMessage& message) {
  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlErrorMessage(message.message.c_str(),
                                                  message.id);
}

void CommandBufferProxyImpl::AddDeletionObserver(DeletionObserver* observer) {
  std::unique_ptr<base::AutoLock> lock;
  if (lock_)
    lock.reset(new base::AutoLock(*lock_));
  deletion_observers_.AddObserver(observer);
}

void CommandBufferProxyImpl::RemoveDeletionObserver(
    DeletionObserver* observer) {
  std::unique_ptr<base::AutoLock> lock;
  if (lock_)
    lock.reset(new base::AutoLock(*lock_));
  deletion_observers_.RemoveObserver(observer);
}

void CommandBufferProxyImpl::OnSignalAck(uint32_t id,
                                         const CommandBuffer::State& state) {
  {
    base::AutoLock lock(last_state_lock_);
    SetStateFromMessageReply(state);
    if (last_state_.error != gpu::error::kNoError)
      return;
  }
  SignalTaskMap::iterator it = signal_tasks_.find(id);
  if (it == signal_tasks_.end()) {
    LOG(ERROR) << "Gpu process sent invalid SignalAck.";
    base::AutoLock lock(last_state_lock_);
    OnGpuAsyncMessageError(gpu::error::kInvalidGpuMessage,
                           gpu::error::kLostContext);
    return;
  }
  base::OnceClosure callback = std::move(it->second);
  signal_tasks_.erase(it);
  std::move(callback).Run();
}

CommandBuffer::State CommandBufferProxyImpl::GetLastState() {
  base::AutoLock lock(last_state_lock_);
  TryUpdateState();
  return last_state_;
}

void CommandBufferProxyImpl::Flush(int32_t put_offset) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  TRACE_EVENT1("gpu", "CommandBufferProxyImpl::Flush", "put_offset",
               put_offset);

  OrderingBarrierHelper(put_offset);

  // Don't send messages once disconnected.
  if (!disconnected_)
    channel_->EnsureFlush(last_flush_id_);
}

void CommandBufferProxyImpl::OrderingBarrier(int32_t put_offset) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  TRACE_EVENT1("gpu", "CommandBufferProxyImpl::OrderingBarrier", "put_offset",
               put_offset);

  OrderingBarrierHelper(put_offset);
}

void CommandBufferProxyImpl::OrderingBarrierHelper(int32_t put_offset) {
  DCHECK(has_buffer_);

  if (last_put_offset_ == put_offset)
    return;
  last_put_offset_ = put_offset;
  last_flush_id_ =
      channel_->OrderingBarrier(route_id_, put_offset, snapshot_requested_,
                                std::move(pending_sync_token_fences_));

  snapshot_requested_ = false;
  pending_sync_token_fences_.clear();

  flushed_fence_sync_release_ = next_fence_sync_release_ - 1;
}

void CommandBufferProxyImpl::SetSwapBuffersCompletionCallback(
    const SwapBuffersCompletionCallback& callback) {
  CheckLock();
  swap_buffers_completion_callback_ = callback;
}

void CommandBufferProxyImpl::SetUpdateVSyncParametersCallback(
    const UpdateVSyncParametersCallback& callback) {
  CheckLock();
  update_vsync_parameters_completion_callback_ = callback;
}

void CommandBufferProxyImpl::SetPresentationCallback(
    const PresentationCallback& callback) {
  CheckLock();
  presentation_callback_ = callback;
}

void CommandBufferProxyImpl::SetNeedsVSync(bool needs_vsync) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_SetNeedsVSync(route_id_, needs_vsync));
}

gpu::CommandBuffer::State CommandBufferProxyImpl::WaitForTokenInRange(
    int32_t start,
    int32_t end) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  TRACE_EVENT2("gpu", "CommandBufferProxyImpl::WaitForToken", "start", start,
               "end", end);
  // Error needs to be checked in case the state was updated on another thread.
  // We need to make sure that the reentrant context loss callback is called so
  // that the share group is also lost before we return any error up the stack.
  if (last_state_.error != gpu::error::kNoError) {
    if (gpu_control_client_)
      gpu_control_client_->OnGpuControlLostContextMaybeReentrant();
    return last_state_;
  }
  TryUpdateState();
  if (!InRange(start, end, last_state_.token) &&
      last_state_.error == gpu::error::kNoError) {
    gpu::CommandBuffer::State state;
    if (Send(new GpuCommandBufferMsg_WaitForTokenInRange(route_id_, start, end,
                                                         &state))) {
      SetStateFromMessageReply(state);
    }
  }
  if (!InRange(start, end, last_state_.token) &&
      last_state_.error == gpu::error::kNoError) {
    LOG(ERROR) << "GPU state invalid after WaitForTokenInRange.";
    OnGpuSyncReplyError();
  }
  return last_state_;
}

gpu::CommandBuffer::State CommandBufferProxyImpl::WaitForGetOffsetInRange(
    uint32_t set_get_buffer_count,
    int32_t start,
    int32_t end) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  TRACE_EVENT2("gpu", "CommandBufferProxyImpl::WaitForGetOffset", "start",
               start, "end", end);
  // Error needs to be checked in case the state was updated on another thread.
  // We need to make sure that the reentrant context loss callback is called so
  // that the share group is also lost before we return any error up the stack.
  if (last_state_.error != gpu::error::kNoError) {
    if (gpu_control_client_)
      gpu_control_client_->OnGpuControlLostContextMaybeReentrant();
    return last_state_;
  }
  TryUpdateState();
  if (((set_get_buffer_count != last_state_.set_get_buffer_count) ||
       !InRange(start, end, last_state_.get_offset)) &&
      last_state_.error == gpu::error::kNoError) {
    gpu::CommandBuffer::State state;
    if (Send(new GpuCommandBufferMsg_WaitForGetOffsetInRange(
            route_id_, set_get_buffer_count, start, end, &state)))
      SetStateFromMessageReply(state);
  }
  if (((set_get_buffer_count != last_state_.set_get_buffer_count) ||
       !InRange(start, end, last_state_.get_offset)) &&
      last_state_.error == gpu::error::kNoError) {
    LOG(ERROR) << "GPU state invalid after WaitForGetOffsetInRange.";
    OnGpuSyncReplyError();
  }
  return last_state_;
}

void CommandBufferProxyImpl::SetGetBuffer(int32_t shm_id) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_SetGetBuffer(route_id_, shm_id));
  last_put_offset_ = -1;
  has_buffer_ = (shm_id > 0);
}

scoped_refptr<gpu::Buffer> CommandBufferProxyImpl::CreateTransferBuffer(
    size_t size,
    int32_t* id) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  *id = -1;

  int32_t new_id = channel_->ReserveTransferBufferId();

  std::unique_ptr<base::SharedMemory> shared_memory =
      AllocateAndMapSharedMemory(size);
  if (!shared_memory) {
    if (last_state_.error == gpu::error::kNoError)
      OnClientError(gpu::error::kOutOfBounds);
    return nullptr;
  }

  if (last_state_.error == gpu::error::kNoError) {
    // This handle is owned by the GPU process and must be passed to it or it
    // will leak. In otherwords, do not early out on error between here and the
    // sending of the RegisterTransferBuffer IPC below.
    base::SharedMemoryHandle handle =
        channel_->ShareToGpuProcess(shared_memory->handle());
    if (!base::SharedMemory::IsHandleValid(handle)) {
      if (last_state_.error == gpu::error::kNoError)
        OnClientError(gpu::error::kLostContext);
      return nullptr;
    }
    Send(new GpuCommandBufferMsg_RegisterTransferBuffer(route_id_, new_id,
                                                        handle, size));
  }

  *id = new_id;
  scoped_refptr<gpu::Buffer> buffer(
      gpu::MakeBufferFromSharedMemory(std::move(shared_memory), size));
  return buffer;
}

void CommandBufferProxyImpl::DestroyTransferBuffer(int32_t id) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_DestroyTransferBuffer(route_id_, id));
}

void CommandBufferProxyImpl::SetGpuControlClient(GpuControlClient* client) {
  CheckLock();
  gpu_control_client_ = client;
}

const gpu::Capabilities& CommandBufferProxyImpl::GetCapabilities() const {
  return capabilities_;
}

int32_t CommandBufferProxyImpl::CreateImage(ClientBuffer buffer,
                                            size_t width,
                                            size_t height,
                                            unsigned internal_format) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return -1;

  int32_t new_id = channel_->ReserveImageId();

  gfx::GpuMemoryBuffer* gpu_memory_buffer =
      reinterpret_cast<gfx::GpuMemoryBuffer*>(buffer);
  DCHECK(gpu_memory_buffer);

  // This handle is owned by the GPU process and must be passed to it or it
  // will leak. In otherwords, do not early out on error between here and the
  // sending of the CreateImage IPC below.
  gfx::GpuMemoryBufferHandle handle =
      gfx::CloneHandleForIPC(gpu_memory_buffer->GetHandle());
  bool requires_sync_token = handle.type == gfx::IO_SURFACE_BUFFER;

  uint64_t image_fence_sync = 0;
  if (requires_sync_token) {
    image_fence_sync = GenerateFenceSyncRelease();

    // Make sure fence syncs were flushed before CreateImage() was called.
    DCHECK_EQ(image_fence_sync, flushed_fence_sync_release_ + 1);
  }

  DCHECK(gpu::IsImageFromGpuMemoryBufferFormatSupported(
      gpu_memory_buffer->GetFormat(), capabilities_));
  DCHECK(gpu::IsImageSizeValidForGpuMemoryBufferFormat(
      gfx::Size(width, height), gpu_memory_buffer->GetFormat()));
  DCHECK(gpu::IsImageFormatCompatibleWithGpuMemoryBufferFormat(
      internal_format, gpu_memory_buffer->GetFormat()));

  GpuCommandBufferMsg_CreateImage_Params params;
  params.id = new_id;
  params.gpu_memory_buffer = handle;
  params.size = gfx::Size(width, height);
  params.format = gpu_memory_buffer->GetFormat();
  params.internal_format = internal_format;
  params.image_release_count = image_fence_sync;

  Send(new GpuCommandBufferMsg_CreateImage(route_id_, params));

  if (image_fence_sync) {
    gpu::SyncToken sync_token(GetNamespaceID(), GetCommandBufferID(),
                              image_fence_sync);

    // Force a synchronous IPC to validate sync token.
    EnsureWorkVisible();
    sync_token.SetVerifyFlush();

    gpu_memory_buffer_manager_->SetDestructionSyncToken(gpu_memory_buffer,
                                                        sync_token);
  }

  return new_id;
}

void CommandBufferProxyImpl::DestroyImage(int32_t id) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_DestroyImage(route_id_, id));
}

uint32_t CommandBufferProxyImpl::CreateStreamTexture(uint32_t texture_id) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return 0;

  int32_t stream_id = channel_->GenerateRouteID();
  bool succeeded = false;
  Send(new GpuCommandBufferMsg_CreateStreamTexture(route_id_, texture_id,
                                                   stream_id, &succeeded));
  if (!succeeded) {
    DLOG(ERROR) << "GpuCommandBufferMsg_CreateStreamTexture returned failure";
    return 0;
  }
  return stream_id;
}

void CommandBufferProxyImpl::SetLock(base::Lock* lock) {
  lock_ = lock;
}

void CommandBufferProxyImpl::EnsureWorkVisible() {
  // Don't send messages once disconnected.
  if (!disconnected_)
    channel_->VerifyFlush(UINT32_MAX);
}

gpu::CommandBufferNamespace CommandBufferProxyImpl::GetNamespaceID() const {
  return gpu::CommandBufferNamespace::GPU_IO;
}

gpu::CommandBufferId CommandBufferProxyImpl::GetCommandBufferID() const {
  return command_buffer_id_;
}

void CommandBufferProxyImpl::FlushPendingWork() {
  // Don't send messages once disconnected.
  if (!disconnected_)
    channel_->EnsureFlush(UINT32_MAX);
}

uint64_t CommandBufferProxyImpl::GenerateFenceSyncRelease() {
  CheckLock();
  return next_fence_sync_release_++;
}

// This can be called from any thread without holding |lock_|. Use a thread-safe
// non-error throwing variant of TryUpdateState for this.
bool CommandBufferProxyImpl::IsFenceSyncReleased(uint64_t release) {
  base::AutoLock lock(last_state_lock_);
  TryUpdateStateThreadSafe();
  return release <= last_state_.release_count;
}

void CommandBufferProxyImpl::SignalSyncToken(const gpu::SyncToken& sync_token,
                                             base::OnceClosure callback) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  uint32_t signal_id = next_signal_id_++;
  Send(new GpuCommandBufferMsg_SignalSyncToken(route_id_, sync_token,
                                               signal_id));
  signal_tasks_.insert(std::make_pair(signal_id, std::move(callback)));
}

void CommandBufferProxyImpl::WaitSyncTokenHint(
    const gpu::SyncToken& sync_token) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  pending_sync_token_fences_.push_back(sync_token);
}

bool CommandBufferProxyImpl::CanWaitUnverifiedSyncToken(
    const gpu::SyncToken& sync_token) {
  // Can only wait on an unverified sync token if it is from the same channel.
  int sync_token_channel_id = GetChannelID(sync_token.command_buffer_id());
  if (sync_token.namespace_id() != gpu::CommandBufferNamespace::GPU_IO ||
      sync_token_channel_id != channel_id_) {
    return false;
  }
  return true;
}

void CommandBufferProxyImpl::SetSnapshotRequested() {
  CheckLock();
  snapshot_requested_ = true;
}

void CommandBufferProxyImpl::SignalQuery(uint32_t query,
                                         base::OnceClosure callback) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  // Signal identifiers are hidden, so nobody outside of this class will see
  // them. (And thus, they cannot save them.) The IDs themselves only last
  // until the callback is invoked, which will happen as soon as the GPU
  // catches upwith the command buffer.
  // A malicious caller trying to create a collision by making next_signal_id
  // would have to make calls at an astounding rate (300B/s) and even if they
  // could do that, all they would do is to prevent some callbacks from getting
  // called, leading to stalled threads and/or memory leaks.
  uint32_t signal_id = next_signal_id_++;
  Send(new GpuCommandBufferMsg_SignalQuery(route_id_, query, signal_id));
  signal_tasks_.insert(std::make_pair(signal_id, std::move(callback)));
}

void CommandBufferProxyImpl::CreateGpuFence(uint32_t gpu_fence_id,
                                            ClientGpuFence source) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError) {
    DLOG(ERROR) << "got error=" << last_state_.error;
    return;
  }

  gfx::GpuFence* gpu_fence = gfx::GpuFence::FromClientGpuFence(source);
  gfx::GpuFenceHandle handle =
      gfx::CloneHandleForIPC(gpu_fence->GetGpuFenceHandle());
  Send(new GpuCommandBufferMsg_CreateGpuFenceFromHandle(route_id_, gpu_fence_id,
                                                        handle));
}

void CommandBufferProxyImpl::GetGpuFence(
    uint32_t gpu_fence_id,
    base::OnceCallback<void(std::unique_ptr<gfx::GpuFence>)> callback) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError) {
    DLOG(ERROR) << "got error=" << last_state_.error;
    return;
  }

  Send(new GpuCommandBufferMsg_GetGpuFenceHandle(route_id_, gpu_fence_id));
  get_gpu_fence_tasks_.emplace(gpu_fence_id, std::move(callback));
}

void CommandBufferProxyImpl::OnGetGpuFenceHandleComplete(
    uint32_t gpu_fence_id,
    const gfx::GpuFenceHandle& handle) {
  // Always consume the provided handle to avoid leaks on error.
  auto gpu_fence = std::make_unique<gfx::GpuFence>(handle);

  GetGpuFenceTaskMap::iterator it = get_gpu_fence_tasks_.find(gpu_fence_id);
  if (it == get_gpu_fence_tasks_.end()) {
    DLOG(ERROR) << "GPU process sent invalid GetGpuFenceHandle response.";
    base::AutoLock lock(last_state_lock_);
    OnGpuAsyncMessageError(gpu::error::kInvalidGpuMessage,
                           gpu::error::kLostContext);
    return;
  }
  auto callback = std::move(it->second);
  get_gpu_fence_tasks_.erase(it);
  std::move(callback).Run(std::move(gpu_fence));
}

void CommandBufferProxyImpl::TakeFrontBuffer(const gpu::Mailbox& mailbox) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_TakeFrontBuffer(route_id_, mailbox));
}

void CommandBufferProxyImpl::ReturnFrontBuffer(const gpu::Mailbox& mailbox,
                                               const gpu::SyncToken& sync_token,
                                               bool is_lost) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_WaitSyncToken(route_id_, sync_token));
  Send(new GpuCommandBufferMsg_ReturnFrontBuffer(route_id_, mailbox, is_lost));
}

bool CommandBufferProxyImpl::Send(IPC::Message* msg) {
  DCHECK(channel_);
  last_state_lock_.AssertAcquired();
  DCHECK_EQ(gpu::error::kNoError, last_state_.error);

  last_state_lock_.Release();

  // Call is_sync() before sending message.
  bool is_sync = msg->is_sync();
  bool result = channel_->Send(msg);
  // Send() should always return true for async messages.
  DCHECK(is_sync || result);

  last_state_lock_.Acquire();

  if (last_state_.error != gpu::error::kNoError) {
    // Error needs to be checked in case the state was updated on another thread
    // while we were waiting on Send. We need to make sure that the reentrant
    // context loss callback is called so that the share group is also lost
    // before we return any error up the stack.
    if (gpu_control_client_)
      gpu_control_client_->OnGpuControlLostContextMaybeReentrant();
    return false;
  }

  if (!result) {
    // Flag the command buffer as lost. Defer deleting the channel until
    // OnChannelError is called after returning to the message loop in case it
    // is referenced elsewhere.
    DVLOG(1) << "CommandBufferProxyImpl::Send failed. Losing context.";
    OnClientError(gpu::error::kLostContext);
    return false;
  }

  return true;
}

std::unique_ptr<base::SharedMemory>
CommandBufferProxyImpl::AllocateAndMapSharedMemory(size_t size) {
  mojo::ScopedSharedBufferHandle handle =
      mojo::SharedBufferHandle::Create(size);
  if (!handle.is_valid()) {
    DLOG(ERROR) << "AllocateAndMapSharedMemory: Create failed";
    return nullptr;
  }

  base::SharedMemoryHandle platform_handle;
  size_t shared_memory_size;
  mojo::UnwrappedSharedMemoryHandleProtection protection;
  MojoResult result = mojo::UnwrapSharedMemoryHandle(
      std::move(handle), &platform_handle, &shared_memory_size, &protection);
  if (result != MOJO_RESULT_OK) {
    DLOG(ERROR) << "AllocateAndMapSharedMemory: Unwrap failed";
    return nullptr;
  }
  DCHECK_EQ(shared_memory_size, size);

  bool read_only =
      protection == mojo::UnwrappedSharedMemoryHandleProtection::kReadOnly;
  auto shm = std::make_unique<base::SharedMemory>(platform_handle, read_only);
  if (!shm->Map(size)) {
    DLOG(ERROR) << "AllocateAndMapSharedMemory: Map failed";
    return nullptr;
  }

  return shm;
}

void CommandBufferProxyImpl::SetStateFromMessageReply(
    const gpu::CommandBuffer::State& state) {
  CheckLock();
  last_state_lock_.AssertAcquired();
  if (last_state_.error != gpu::error::kNoError)
    return;
  // Handle wraparound. It works as long as we don't have more than 2B state
  // updates in flight across which reordering occurs.
  if (state.generation - last_state_.generation < 0x80000000U)
    last_state_ = state;
  if (last_state_.error != gpu::error::kNoError)
    OnGpuStateError();
}

void CommandBufferProxyImpl::TryUpdateState() {
  CheckLock();
  last_state_lock_.AssertAcquired();
  if (last_state_.error == gpu::error::kNoError) {
    shared_state()->Read(&last_state_);
    if (last_state_.error != gpu::error::kNoError)
      OnGpuStateError();
  }
}

void CommandBufferProxyImpl::TryUpdateStateThreadSafe() {
  last_state_lock_.AssertAcquired();
  if (last_state_.error == gpu::error::kNoError) {
    shared_state()->Read(&last_state_);
    if (last_state_.error != gpu::error::kNoError) {
      callback_thread_->PostTask(
          FROM_HERE,
          base::Bind(&CommandBufferProxyImpl::LockAndDisconnectChannel,
                     weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void CommandBufferProxyImpl::TryUpdateStateDontReportError() {
  last_state_lock_.AssertAcquired();
  if (last_state_.error == gpu::error::kNoError)
    shared_state()->Read(&last_state_);
}

gpu::CommandBufferSharedState* CommandBufferProxyImpl::shared_state() const {
  return reinterpret_cast<gpu::CommandBufferSharedState*>(
      shared_state_shm_->memory());
}

void CommandBufferProxyImpl::OnSwapBuffersCompleted(
    const SwapBuffersCompleteParams& params) {
  if (!swap_buffers_completion_callback_.is_null())
    swap_buffers_completion_callback_.Run(params);
}

void CommandBufferProxyImpl::OnUpdateVSyncParameters(base::TimeTicks timebase,
                                                     base::TimeDelta interval) {
  DCHECK(!gl::IsPresentationCallbackEnabled());
  if (!update_vsync_parameters_completion_callback_.is_null())
    update_vsync_parameters_completion_callback_.Run(timebase, interval);
}

void CommandBufferProxyImpl::OnBufferPresented(
    uint64_t swap_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(gl::IsPresentationCallbackEnabled());
  if (presentation_callback_)
    presentation_callback_.Run(swap_id, feedback);

  if (update_vsync_parameters_completion_callback_ &&
      feedback.timestamp != base::TimeTicks()) {
    update_vsync_parameters_completion_callback_.Run(feedback.timestamp,
                                                     feedback.interval);
  }
}

void CommandBufferProxyImpl::OnGpuSyncReplyError() {
  CheckLock();
  last_state_lock_.AssertAcquired();
  last_state_.error = gpu::error::kLostContext;
  last_state_.context_lost_reason = gpu::error::kInvalidGpuMessage;
  // This method may be inside a callstack from the GpuControlClient (we got a
  // bad reply to something we are sending to the GPU process). So avoid
  // re-entering the GpuControlClient here.
  DisconnectChannelInFreshCallStack();
}

void CommandBufferProxyImpl::OnGpuAsyncMessageError(
    gpu::error::ContextLostReason reason,
    gpu::error::Error error) {
  CheckLock();
  last_state_lock_.AssertAcquired();
  last_state_.error = error;
  last_state_.context_lost_reason = reason;
  // This method only occurs when receiving IPC messages, so we know it's not in
  // a callstack from the GpuControlClient. Unlock the state lock to prevent
  // a deadlock when calling the context loss callback.
  base::AutoUnlock unlock(last_state_lock_);
  DisconnectChannel();
}

void CommandBufferProxyImpl::OnGpuStateError() {
  CheckLock();
  last_state_lock_.AssertAcquired();
  DCHECK_NE(gpu::error::kNoError, last_state_.error);
  // This method may be inside a callstack from the GpuControlClient (we
  // encountered an error while trying to perform some action). So avoid
  // re-entering the GpuControlClient here.
  DisconnectChannelInFreshCallStack();
}

void CommandBufferProxyImpl::OnClientError(gpu::error::Error error) {
  CheckLock();
  last_state_lock_.AssertAcquired();
  last_state_.error = error;
  last_state_.context_lost_reason = gpu::error::kUnknown;
  // This method may be inside a callstack from the GpuControlClient (we
  // encountered an error while trying to perform some action). So avoid
  // re-entering the GpuControlClient here.
  DisconnectChannelInFreshCallStack();
}

void CommandBufferProxyImpl::DisconnectChannelInFreshCallStack() {
  CheckLock();
  last_state_lock_.AssertAcquired();
  // Inform the GpuControlClient of the lost state immediately, though this may
  // be a re-entrant call to the client so we use the MaybeReentrant variant.
  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlLostContextMaybeReentrant();
  // Create a fresh call stack to keep the |channel_| alive while we unwind the
  // stack in case things will use it, and give the GpuChannelClient a chance to
  // act fully on the lost context.
  callback_thread_->PostTask(
      FROM_HERE, base::Bind(&CommandBufferProxyImpl::LockAndDisconnectChannel,
                            weak_ptr_factory_.GetWeakPtr()));
}

void CommandBufferProxyImpl::LockAndDisconnectChannel() {
  base::Optional<base::AutoLock> hold;
  if (lock_)
    hold.emplace(*lock_);
  DisconnectChannel();
}

void CommandBufferProxyImpl::DisconnectChannel() {
  CheckLock();
  // Prevent any further messages from being sent, and ensure we only call
  // the client for lost context a single time.
  if (!channel_ || disconnected_)
    return;
  disconnected_ = true;
  channel_->VerifyFlush(UINT32_MAX);
  channel_->Send(new GpuChannelMsg_DestroyCommandBuffer(route_id_));
  channel_->RemoveRoute(route_id_);
  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlLostContext();
}

}  // namespace gpu
