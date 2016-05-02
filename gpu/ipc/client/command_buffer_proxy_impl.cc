// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/command_buffer_proxy_impl.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
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

namespace gpu {

namespace {

gpu::CommandBufferId CommandBufferProxyID(int channel_id, int32_t route_id) {
  return gpu::CommandBufferId::FromUnsafeValue(
      (static_cast<uint64_t>(channel_id) << 32) | route_id);
}

}  // namespace

CommandBufferProxyImpl::CommandBufferProxyImpl(
    scoped_refptr<GpuChannelHost> channel,
    int32_t route_id,
    int32_t stream_id)
    : lock_(nullptr),
      gpu_control_client_(nullptr),
      channel_(std::move(channel)),
      command_buffer_id_(
          CommandBufferProxyID(channel_->channel_id(), route_id)),
      route_id_(route_id),
      stream_id_(stream_id),
      flush_count_(0),
      last_put_offset_(-1),
      last_barrier_put_offset_(-1),
      next_fence_sync_release_(1),
      flushed_fence_sync_release_(0),
      verified_fence_sync_release_(0),
      next_signal_id_(0),
      weak_this_(AsWeakPtr()),
      callback_thread_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(channel_);
  DCHECK(stream_id);
}

CommandBufferProxyImpl::~CommandBufferProxyImpl() {
  FOR_EACH_OBSERVER(DeletionObserver, deletion_observers_, OnWillDeleteImpl());
  if (channel_) {
    channel_->DestroyCommandBuffer(this);
    channel_ = nullptr;
  }
}

bool CommandBufferProxyImpl::OnMessageReceived(const IPC::Message& message) {
  std::unique_ptr<base::AutoLock> lock;
  if (lock_)
    lock.reset(new base::AutoLock(*lock_));
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
    OnGpuAsyncMessageError(gpu::error::kInvalidGpuMessage,
                           gpu::error::kLostContext);
  }
  return handled;
}

void CommandBufferProxyImpl::OnChannelError() {
  std::unique_ptr<base::AutoLock> lock;
  if (lock_)
    lock.reset(new base::AutoLock(*lock_));

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

void CommandBufferProxyImpl::OnSignalAck(uint32_t id) {
  SignalTaskMap::iterator it = signal_tasks_.find(id);
  if (it == signal_tasks_.end()) {
    LOG(ERROR) << "Gpu process sent invalid SignalAck.";
    OnGpuAsyncMessageError(gpu::error::kInvalidGpuMessage,
                           gpu::error::kLostContext);
    return;
  }
  base::Closure callback = it->second;
  signal_tasks_.erase(it);
  callback.Run();
}

bool CommandBufferProxyImpl::Initialize() {
  TRACE_EVENT0("gpu", "CommandBufferProxyImpl::Initialize");
  shared_state_shm_.reset(channel_->factory()
                              ->AllocateSharedMemory(sizeof(*shared_state()))
                              .release());
  if (!shared_state_shm_)
    return false;

  if (!shared_state_shm_->Map(sizeof(*shared_state())))
    return false;

  shared_state()->Initialize();

  // This handle is owned by the GPU process and must be passed to it or it
  // will leak. In otherwords, do not early out on error between here and the
  // sending of the Initialize IPC below.
  base::SharedMemoryHandle handle =
      channel_->ShareToGpuProcess(shared_state_shm_->handle());
  if (!base::SharedMemory::IsHandleValid(handle))
    return false;

  bool result = false;
  if (!Send(new GpuCommandBufferMsg_Initialize(route_id_, handle, &result,
                                               &capabilities_))) {
    LOG(ERROR) << "Could not send GpuCommandBufferMsg_Initialize.";
    return false;
  }

  if (!result) {
    LOG(ERROR) << "Failed to initialize command buffer service.";
    return false;
  }

  capabilities_.image = true;

  return true;
}

gpu::CommandBuffer::State CommandBufferProxyImpl::GetLastState() {
  return last_state_;
}

int32_t CommandBufferProxyImpl::GetLastToken() {
  TryUpdateState();
  return last_state_.token;
}

void CommandBufferProxyImpl::Flush(int32_t put_offset) {
  CheckLock();
  if (last_state_.error != gpu::error::kNoError)
    return;

  TRACE_EVENT1("gpu", "CommandBufferProxyImpl::Flush", "put_offset",
               put_offset);

  bool put_offset_changed = last_put_offset_ != put_offset;
  last_put_offset_ = put_offset;
  last_barrier_put_offset_ = put_offset;

  if (channel_) {
    const uint32_t flush_id = channel_->OrderingBarrier(
        route_id_, stream_id_, put_offset, ++flush_count_, latency_info_,
        put_offset_changed, true);
    if (put_offset_changed) {
      DCHECK(flush_id);
      const uint64_t fence_sync_release = next_fence_sync_release_ - 1;
      if (fence_sync_release > flushed_fence_sync_release_) {
        flushed_fence_sync_release_ = fence_sync_release;
        flushed_release_flush_id_.push(
            std::make_pair(fence_sync_release, flush_id));
      }
    }
  }

  if (put_offset_changed)
    latency_info_.clear();
}

void CommandBufferProxyImpl::OrderingBarrier(int32_t put_offset) {
  CheckLock();
  if (last_state_.error != gpu::error::kNoError)
    return;

  TRACE_EVENT1("gpu", "CommandBufferProxyImpl::OrderingBarrier", "put_offset",
               put_offset);

  bool put_offset_changed = last_barrier_put_offset_ != put_offset;
  last_barrier_put_offset_ = put_offset;

  if (channel_) {
    const uint32_t flush_id = channel_->OrderingBarrier(
        route_id_, stream_id_, put_offset, ++flush_count_, latency_info_,
        put_offset_changed, false);
    if (put_offset_changed) {
      DCHECK(flush_id);
      const uint64_t fence_sync_release = next_fence_sync_release_ - 1;
      if (fence_sync_release > flushed_fence_sync_release_) {
        flushed_fence_sync_release_ = fence_sync_release;
        flushed_release_flush_id_.push(
            std::make_pair(fence_sync_release, flush_id));
      }
    }
  }

  if (put_offset_changed)
    latency_info_.clear();
}

void CommandBufferProxyImpl::SetLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  CheckLock();
  for (size_t i = 0; i < latency_info.size(); i++)
    latency_info_.push_back(latency_info[i]);
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

void CommandBufferProxyImpl::WaitForTokenInRange(int32_t start, int32_t end) {
  CheckLock();
  TRACE_EVENT2("gpu", "CommandBufferProxyImpl::WaitForToken", "start", start,
               "end", end);
  TryUpdateState();
  if (!InRange(start, end, last_state_.token) &&
      last_state_.error == gpu::error::kNoError) {
    gpu::CommandBuffer::State state;
    if (Send(new GpuCommandBufferMsg_WaitForTokenInRange(route_id_, start, end,
                                                         &state)))
      SetStateFromSyncReply(state);
  }
  if (!InRange(start, end, last_state_.token) &&
      last_state_.error == gpu::error::kNoError) {
    LOG(ERROR) << "GPU state invalid after WaitForTokenInRange.";
    OnGpuSyncReplyError();
  }
}

void CommandBufferProxyImpl::WaitForGetOffsetInRange(int32_t start,
                                                     int32_t end) {
  CheckLock();
  TRACE_EVENT2("gpu", "CommandBufferProxyImpl::WaitForGetOffset", "start",
               start, "end", end);
  TryUpdateState();
  if (!InRange(start, end, last_state_.get_offset) &&
      last_state_.error == gpu::error::kNoError) {
    gpu::CommandBuffer::State state;
    if (Send(new GpuCommandBufferMsg_WaitForGetOffsetInRange(route_id_, start,
                                                             end, &state)))
      SetStateFromSyncReply(state);
  }
  if (!InRange(start, end, last_state_.get_offset) &&
      last_state_.error == gpu::error::kNoError) {
    LOG(ERROR) << "GPU state invalid after WaitForGetOffsetInRange.";
    OnGpuSyncReplyError();
  }
}

void CommandBufferProxyImpl::SetGetBuffer(int32_t shm_id) {
  CheckLock();
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_SetGetBuffer(route_id_, shm_id));
  last_put_offset_ = -1;
}

scoped_refptr<gpu::Buffer> CommandBufferProxyImpl::CreateTransferBuffer(
    size_t size,
    int32_t* id) {
  CheckLock();
  *id = -1;

  if (last_state_.error != gpu::error::kNoError)
    return NULL;

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

  Send(new GpuCommandBufferMsg_RegisterTransferBuffer(route_id_, new_id, handle,
                                                      size));
  *id = new_id;
  scoped_refptr<gpu::Buffer> buffer(
      gpu::MakeBufferFromSharedMemory(std::move(shared_memory), size));
  return buffer;
}

void CommandBufferProxyImpl::DestroyTransferBuffer(int32_t id) {
  CheckLock();
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_DestroyTransferBuffer(route_id_, id));
}

void CommandBufferProxyImpl::SetGpuControlClient(GpuControlClient* client) {
  CheckLock();
  gpu_control_client_ = client;
}

gpu::Capabilities CommandBufferProxyImpl::GetCapabilities() {
  return capabilities_;
}

int32_t CommandBufferProxyImpl::CreateImage(ClientBuffer buffer,
                                            size_t width,
                                            size_t height,
                                            unsigned internal_format) {
  CheckLock();
  if (last_state_.error != gpu::error::kNoError)
    return -1;

  int32_t new_id = channel_->ReserveImageId();

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
      channel_->gpu_memory_buffer_manager();
  gfx::GpuMemoryBuffer* gpu_memory_buffer =
      gpu_memory_buffer_manager->GpuMemoryBufferFromClientBuffer(buffer);
  DCHECK(gpu_memory_buffer);

  // This handle is owned by the GPU process and must be passed to it or it
  // will leak. In otherwords, do not early out on error between here and the
  // sending of the CreateImage IPC below.
  bool requires_sync_token = false;
  gfx::GpuMemoryBufferHandle handle =
      channel_->ShareGpuMemoryBufferToGpuProcess(gpu_memory_buffer->GetHandle(),
                                                 &requires_sync_token);

  uint64_t image_fence_sync = 0;
  if (requires_sync_token) {
    image_fence_sync = GenerateFenceSyncRelease();

    // Make sure fence syncs were flushed before CreateImage() was called.
    DCHECK_EQ(image_fence_sync, flushed_fence_sync_release_ + 1);
  }

  DCHECK(gpu::IsGpuMemoryBufferFormatSupported(gpu_memory_buffer->GetFormat(),
                                               capabilities_));
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
    gpu::SyncToken sync_token(GetNamespaceID(), GetExtraCommandBufferData(),
                              GetCommandBufferID(), image_fence_sync);

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
  if (last_state_.error != gpu::error::kNoError)
    return;

  Send(new GpuCommandBufferMsg_DestroyImage(route_id_, id));
}

int32_t CommandBufferProxyImpl::CreateGpuMemoryBufferImage(
    size_t width,
    size_t height,
    unsigned internal_format,
    unsigned usage) {
  CheckLock();
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer(
      channel_->gpu_memory_buffer_manager()->AllocateGpuMemoryBuffer(
          gfx::Size(width, height),
          gpu::DefaultBufferFormatForImageFormat(internal_format),
          gfx::BufferUsage::SCANOUT, gpu::kNullSurfaceHandle));
  if (!buffer)
    return -1;

  return CreateImage(buffer->AsClientBuffer(), width, height, internal_format);
}

uint32_t CommandBufferProxyImpl::CreateStreamTexture(uint32_t texture_id) {
  CheckLock();
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
  if (channel_)
    channel_->ValidateFlushIDReachedServer(stream_id_, true);
}

gpu::CommandBufferNamespace CommandBufferProxyImpl::GetNamespaceID() const {
  return gpu::CommandBufferNamespace::GPU_IO;
}

gpu::CommandBufferId CommandBufferProxyImpl::GetCommandBufferID() const {
  return command_buffer_id_;
}

int32_t CommandBufferProxyImpl::GetExtraCommandBufferData() const {
  return stream_id_;
}

uint64_t CommandBufferProxyImpl::GenerateFenceSyncRelease() {
  CheckLock();
  return next_fence_sync_release_++;
}

bool CommandBufferProxyImpl::IsFenceSyncRelease(uint64_t release) {
  CheckLock();
  return release != 0 && release < next_fence_sync_release_;
}

bool CommandBufferProxyImpl::IsFenceSyncFlushed(uint64_t release) {
  CheckLock();
  return release != 0 && release <= flushed_fence_sync_release_;
}

bool CommandBufferProxyImpl::IsFenceSyncFlushReceived(uint64_t release) {
  CheckLock();
  if (last_state_.error != gpu::error::kNoError)
    return false;

  if (release <= verified_fence_sync_release_)
    return true;

  // Check if we have actually flushed the fence sync release.
  if (release <= flushed_fence_sync_release_) {
    DCHECK(!flushed_release_flush_id_.empty());
    // Check if it has already been validated by another context.
    UpdateVerifiedReleases(channel_->GetHighestValidatedFlushID(stream_id_));
    if (release <= verified_fence_sync_release_)
      return true;

    // Has not been validated, validate it now.
    UpdateVerifiedReleases(
        channel_->ValidateFlushIDReachedServer(stream_id_, false));
    return release <= verified_fence_sync_release_;
  }

  return false;
}

void CommandBufferProxyImpl::SignalSyncToken(const gpu::SyncToken& sync_token,
                                             const base::Closure& callback) {
  CheckLock();
  if (last_state_.error != gpu::error::kNoError)
    return;

  uint32_t signal_id = next_signal_id_++;
  Send(new GpuCommandBufferMsg_SignalSyncToken(route_id_, sync_token,
                                               signal_id));
  signal_tasks_.insert(std::make_pair(signal_id, callback));
}

bool CommandBufferProxyImpl::CanWaitUnverifiedSyncToken(
    const gpu::SyncToken* sync_token) {
  // Can only wait on an unverified sync token if it is from the same channel.
  const uint64_t token_channel =
      sync_token->command_buffer_id().GetUnsafeValue() >> 32;
  const uint64_t channel = command_buffer_id_.GetUnsafeValue() >> 32;
  if (sync_token->namespace_id() != gpu::CommandBufferNamespace::GPU_IO ||
      token_channel != channel) {
    return false;
  }

  // If waiting on a different stream, flush pending commands on that stream.
  const int32_t release_stream_id = sync_token->extra_data_field();
  if (release_stream_id == 0)
    return false;

  if (release_stream_id != stream_id_)
    channel_->FlushPendingStream(release_stream_id);

  return true;
}

void CommandBufferProxyImpl::SignalQuery(uint32_t query,
                                         const base::Closure& callback) {
  CheckLock();
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

bool CommandBufferProxyImpl::ProduceFrontBuffer(const gpu::Mailbox& mailbox) {
  CheckLock();
  if (last_state_.error != gpu::error::kNoError)
    return false;

  Send(new GpuCommandBufferMsg_ProduceFrontBuffer(route_id_, mailbox));
  return true;
}

gpu::error::Error CommandBufferProxyImpl::GetLastError() {
  return last_state_.error;
}

bool CommandBufferProxyImpl::Send(IPC::Message* msg) {
  // Caller should not intentionally send a message if the context is lost.
  DCHECK(last_state_.error == gpu::error::kNoError);
  DCHECK(channel_);

  if (!msg->is_sync()) {
    bool result = channel_->Send(msg);
    // Send() should always return true for async messages.
    DCHECK(result);
    return true;
  }

  if (channel_->Send(msg))
    return true;

  // Flag the command buffer as lost. Defer deleting the channel until
  // OnChannelError is called after returning to the message loop in case
  // it is referenced elsewhere.
  DVLOG(1) << "CommandBufferProxyImpl::Send failed. Losing context.";
  OnClientError(gpu::error::kLostContext);
  return false;
}

void CommandBufferProxyImpl::SetStateFromSyncReply(
    const gpu::CommandBuffer::State& state) {
  DCHECK(last_state_.error == gpu::error::kNoError);
  // Handle wraparound. It works as long as we don't have more than 2B state
  // updates in flight across which reordering occurs.
  if (state.generation - last_state_.generation < 0x80000000U)
    last_state_ = state;
  if (last_state_.error != gpu::error::kNoError)
    OnGpuStateError();
}

void CommandBufferProxyImpl::TryUpdateState() {
  if (last_state_.error == gpu::error::kNoError) {
    shared_state()->Read(&last_state_);
    if (last_state_.error != gpu::error::kNoError)
      OnGpuStateError();
  }
}

void CommandBufferProxyImpl::TryUpdateStateDontReportError() {
  if (last_state_.error == gpu::error::kNoError)
    shared_state()->Read(&last_state_);
}

void CommandBufferProxyImpl::UpdateVerifiedReleases(uint32_t verified_flush) {
  while (!flushed_release_flush_id_.empty()) {
    const std::pair<uint64_t, uint32_t>& front_item =
        flushed_release_flush_id_.front();
    if (front_item.second > verified_flush)
      break;
    verified_fence_sync_release_ = front_item.first;
    flushed_release_flush_id_.pop();
  }
}

gpu::CommandBufferSharedState* CommandBufferProxyImpl::shared_state() const {
  return reinterpret_cast<gpu::CommandBufferSharedState*>(
      shared_state_shm_->memory());
}

void CommandBufferProxyImpl::OnSwapBuffersCompleted(
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::SwapResult result) {
  if (!swap_buffers_completion_callback_.is_null()) {
    if (!ui::LatencyInfo::Verify(
            latency_info, "CommandBufferProxyImpl::OnSwapBuffersCompleted")) {
      swap_buffers_completion_callback_.Run(std::vector<ui::LatencyInfo>(),
                                            result);
      return;
    }
    swap_buffers_completion_callback_.Run(latency_info, result);
  }
}

void CommandBufferProxyImpl::OnUpdateVSyncParameters(base::TimeTicks timebase,
                                                     base::TimeDelta interval) {
  if (!update_vsync_parameters_completion_callback_.is_null())
    update_vsync_parameters_completion_callback_.Run(timebase, interval);
}

void CommandBufferProxyImpl::OnGpuSyncReplyError() {
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
  last_state_.error = error;
  last_state_.context_lost_reason = reason;
  // This method only occurs when receiving IPC messages, so we know it's not in
  // a callstack from the GpuControlClient.
  DisconnectChannel();
}

void CommandBufferProxyImpl::OnGpuStateError() {
  DCHECK(last_state_.error != gpu::error::kNoError);
  // This method may be inside a callstack from the GpuControlClient (we
  // encountered an error while trying to perform some action). So avoid
  // re-entering the GpuControlClient here.
  DisconnectChannelInFreshCallStack();
}

void CommandBufferProxyImpl::OnClientError(gpu::error::Error error) {
  CheckLock();
  last_state_.error = error;
  last_state_.context_lost_reason = gpu::error::kUnknown;
  // This method may be inside a callstack from the GpuControlClient (we
  // encountered an error while trying to perform some action). So avoid
  // re-entering the GpuControlClient here.
  DisconnectChannelInFreshCallStack();
}

void CommandBufferProxyImpl::DisconnectChannelInFreshCallStack() {
  CheckLock();
  // Inform the GpuControlClient of the lost state immediately, though this may
  // be a re-entrant call to the client so we use the MaybeReentrant variant.
  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlLostContextMaybeReentrant();
  // Create a fresh call stack to keep the |channel_| alive while we unwind the
  // stack in case things will use it, and give the GpuChannelClient a chance to
  // act fully on the lost context.
  callback_thread_->PostTask(
      FROM_HERE, base::Bind(&CommandBufferProxyImpl::LockAndDisconnectChannel,
                            weak_this_));
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
  if (!channel_)
    return;
  channel_->DestroyCommandBuffer(this);
  channel_ = nullptr;
  if (gpu_control_client_)
    gpu_control_client_->OnGpuControlLostContext();
}

}  // namespace gpu
