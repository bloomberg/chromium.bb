// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/command_buffer_proxy_impl.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
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
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"

#if defined(OS_MACOSX)
#include "gpu/ipc/client/gpu_process_hosted_ca_layer_tree_params.h"
#endif

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

CommandBufferProxyImpl::CommandBufferProxyImpl(int channel_id,
                                               int32_t route_id,
                                               int32_t stream_id)
    : command_buffer_id_(CommandBufferProxyID(channel_id, route_id)),
      channel_id_(channel_id),
      route_id_(route_id),
      stream_id_(stream_id),
      weak_ptr_factory_(this) {
  DCHECK(route_id);
}

// static
std::unique_ptr<CommandBufferProxyImpl> CommandBufferProxyImpl::Create(
    scoped_refptr<GpuChannelHost> host,
    gpu::SurfaceHandle surface_handle,
    CommandBufferProxyImpl* share_group,
    int32_t stream_id,
    gpu::SchedulingPriority stream_priority,
    const gpu::gles2::ContextCreationAttribHelper& attribs,
    const GURL& active_url,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!share_group || (stream_id == share_group->stream_id_));
  TRACE_EVENT1("gpu", "GpuChannelHost::CreateViewCommandBuffer",
               "surface_handle", surface_handle);

  GPUCreateCommandBufferConfig init_params;
  init_params.surface_handle = surface_handle;
  init_params.share_group_id =
      share_group ? share_group->route_id_ : MSG_ROUTING_NONE;
  init_params.stream_id = stream_id;
  init_params.stream_priority = stream_priority;
  init_params.attribs = attribs;
  init_params.active_url = active_url;

  int32_t route_id = host->GenerateRouteID();
  std::unique_ptr<CommandBufferProxyImpl> command_buffer = base::WrapUnique(
      new CommandBufferProxyImpl(host->channel_id(), route_id, stream_id));
  if (!command_buffer->Initialize(std::move(host), std::move(init_params),
                                  std::move(task_runner)))
    return nullptr;

  return command_buffer;
}

CommandBufferProxyImpl::~CommandBufferProxyImpl() {
  for (auto& observer : deletion_observers_)
    observer.OnWillDeleteImpl();
  DisconnectChannel();
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
  base::Closure callback = it->second;
  signal_tasks_.erase(it);
  callback.Run();
}

bool CommandBufferProxyImpl::Initialize(
    scoped_refptr<GpuChannelHost> channel,
    const GPUCreateCommandBufferConfig& config,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!channel_);
  TRACE_EVENT0("gpu", "CommandBufferProxyImpl::Initialize");
  shared_state_shm_ =
      channel->factory()->AllocateSharedMemory(sizeof(*shared_state()));
  if (!shared_state_shm_)
    return false;

  if (!shared_state_shm_->Map(sizeof(*shared_state())))
    return false;

  shared_state()->Initialize();

  // This handle is owned by the GPU process and must be passed to it or it
  // will leak. In otherwords, do not early out on error between here and the
  // sending of the CreateCommandBuffer IPC below.
  base::SharedMemoryHandle handle =
      channel->ShareToGpuProcess(shared_state_shm_->handle());
  if (!base::SharedMemory::IsHandleValid(handle))
    return false;

  // Route must be added before sending the message, otherwise messages sent
  // from the GPU process could race against adding ourselves to the filter.
  channel->AddRouteWithTaskRunner(route_id_, weak_ptr_factory_.GetWeakPtr(),
                                  task_runner);

  // We're blocking the UI thread, which is generally undesirable.
  // In this case we need to wait for this before we can show any UI /anyway/,
  // so it won't cause additional jank.
  // TODO(piman): Make this asynchronous (http://crbug.com/125248).
  bool result = false;
  bool sent = channel->Send(new GpuChannelMsg_CreateCommandBuffer(
      config, route_id_, handle, &result, &capabilities_));
  if (!sent || !result) {
    DLOG(ERROR) << "Failed to send GpuChannelMsg_CreateCommandBuffer.";
    channel->RemoveRoute(route_id_);
    return false;
  }

  channel_ = std::move(channel);
  callback_thread_ = std::move(task_runner);

  return true;
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
      channel_->OrderingBarrier(route_id_, put_offset, std::move(latency_info_),
                                std::move(pending_sync_token_fences_));

  latency_info_.clear();
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

  std::unique_ptr<base::SharedMemory> shared_memory(
      channel_->factory()->AllocateSharedMemory(size));
  if (!shared_memory) {
    if (last_state_.error == gpu::error::kNoError)
      OnClientError(gpu::error::kOutOfBounds);
    return NULL;
  }

  DCHECK(!shared_memory->memory());
  if (!shared_memory->Map(size)) {
    if (last_state_.error == gpu::error::kNoError)
      OnClientError(gpu::error::kOutOfBounds);
    return NULL;
  }

  if (!disconnected_) {
    // This handle is owned by the GPU process and must be passed to it or it
    // will leak. In otherwords, do not early out on error between here and the
    // sending of the RegisterTransferBuffer IPC below.
    base::SharedMemoryHandle handle =
        channel_->ShareToGpuProcess(shared_memory->handle());
    if (!base::SharedMemory::IsHandleValid(handle)) {
      if (last_state_.error == gpu::error::kNoError)
        OnClientError(gpu::error::kLostContext);
      return NULL;
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

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
      channel_->gpu_memory_buffer_manager();
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
    gpu::SyncToken sync_token(GetNamespaceID(), 0, GetCommandBufferID(),
                              image_fence_sync);

    // Force a synchronous IPC to validate sync token.
    EnsureWorkVisible();
    sync_token.SetVerifyFlush();

    gpu_memory_buffer_manager->SetDestructionSyncToken(gpu_memory_buffer,
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

bool CommandBufferProxyImpl::IsFenceSyncRelease(uint64_t release) {
  CheckLock();
  return release && release < next_fence_sync_release_;
}

bool CommandBufferProxyImpl::IsFenceSyncFlushed(uint64_t release) {
  CheckLock();
  return release && release <= flushed_fence_sync_release_;
}

bool CommandBufferProxyImpl::IsFenceSyncFlushReceived(uint64_t release) {
  CheckLock();
  if (release > verified_fence_sync_release_) {
    // Don't send messages once disconnected.
    if (!disconnected_)
      channel_->VerifyFlush(last_flush_id_);
    verified_fence_sync_release_ = flushed_fence_sync_release_;
  }
  return release && release <= verified_fence_sync_release_;
}

// This can be called from any thread without holding |lock_|. Use a thread-safe
// non-error throwing variant of TryUpdateState for this.
bool CommandBufferProxyImpl::IsFenceSyncReleased(uint64_t release) {
  base::AutoLock lock(last_state_lock_);
  TryUpdateStateThreadSafe();
  return release <= last_state_.release_count;
}

void CommandBufferProxyImpl::SignalSyncToken(const gpu::SyncToken& sync_token,
                                             const base::Closure& callback) {
  CheckLock();
  base::AutoLock lock(last_state_lock_);
  if (last_state_.error != gpu::error::kNoError)
    return;

  uint32_t signal_id = next_signal_id_++;
  Send(new GpuCommandBufferMsg_SignalSyncToken(route_id_, sync_token,
                                               signal_id));
  signal_tasks_.insert(std::make_pair(signal_id, callback));
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

void CommandBufferProxyImpl::AddLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  CheckLock();
  for (size_t i = 0; i < latency_info.size(); i++)
    latency_info_.push_back(latency_info[i]);
}

void CommandBufferProxyImpl::SignalQuery(uint32_t query,
                                         const base::Closure& callback) {
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
  signal_tasks_.insert(std::make_pair(signal_id, callback));
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
    const GpuCommandBufferMsg_SwapBuffersCompleted_Params& params) {
#if defined(OS_MACOSX)
  gpu::GpuProcessHostedCALayerTreeParamsMac params_mac;
  params_mac.ca_context_id = params.ca_context_id;
  params_mac.fullscreen_low_power_ca_context_valid =
      params.fullscreen_low_power_ca_context_valid;
  params_mac.fullscreen_low_power_ca_context_id =
      params.fullscreen_low_power_ca_context_id;
  params_mac.io_surface.reset(IOSurfaceLookupFromMachPort(params.io_surface));
  params_mac.pixel_size = params.pixel_size;
  params_mac.scale_factor = params.scale_factor;
  params_mac.responses = std::move(params.in_use_responses);
  gpu::GpuProcessHostedCALayerTreeParamsMac* mac_frame_ptr = &params_mac;
#else
  gpu::GpuProcessHostedCALayerTreeParamsMac* mac_frame_ptr = nullptr;
#endif

  if (!swap_buffers_completion_callback_.is_null()) {
    if (!ui::LatencyInfo::Verify(
            params.latency_info,
            "CommandBufferProxyImpl::OnSwapBuffersCompleted")) {
      swap_buffers_completion_callback_.Run(std::vector<ui::LatencyInfo>(),
                                            params.result, mac_frame_ptr);
    } else {
      swap_buffers_completion_callback_.Run(params.latency_info, params.result,
                                            mac_frame_ptr);
    }
  }
}

void CommandBufferProxyImpl::OnUpdateVSyncParameters(base::TimeTicks timebase,
                                                     base::TimeDelta interval) {
  if (!update_vsync_parameters_completion_callback_.is_null())
    update_vsync_parameters_completion_callback_.Run(timebase, interval);
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
