// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gles2/command_buffer_local.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/memory/shared_memory.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "components/mus/gles2/command_buffer_driver.h"
#include "components/mus/gles2/command_buffer_local_client.h"
#include "components/mus/gles2/command_buffer_type_conversions.h"
#include "components/mus/gles2/gpu_memory_tracker.h"
#include "components/mus/gles2/gpu_state.h"
#include "components/mus/gles2/mojo_buffer_backing.h"
#include "components/mus/gles2/mojo_gpu_memory_buffer.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shader_translator_cache.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/command_buffer/service/valuebuffer_manager.h"
#include "mojo/platform_handle/platform_handle_functions.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_surface.h"

namespace mus {

namespace {

uint64_t g_next_command_buffer_id = 0;

bool CreateMapAndDupSharedBuffer(size_t size,
                                 void** memory,
                                 mojo::ScopedSharedBufferHandle* handle,
                                 mojo::ScopedSharedBufferHandle* duped) {
  MojoResult result = mojo::CreateSharedBuffer(NULL, size, handle);
  if (result != MOJO_RESULT_OK)
    return false;
  DCHECK(handle->is_valid());

  result = mojo::DuplicateBuffer(handle->get(), NULL, duped);
  if (result != MOJO_RESULT_OK)
    return false;
  DCHECK(duped->is_valid());

  result = mojo::MapBuffer(handle->get(), 0, size, memory,
                           MOJO_MAP_BUFFER_FLAG_NONE);
  if (result != MOJO_RESULT_OK)
    return false;
  DCHECK(*memory);

  return true;
}

void PostTask(const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
              const base::Closure& callback) {
  task_runner->PostTask(FROM_HERE, callback);
}
}

const unsigned int GL_READ_WRITE_CHROMIUM = 0x78F2;

CommandBufferLocal::CommandBufferLocal(CommandBufferLocalClient* client,
                                       gfx::AcceleratedWidget widget,
                                       const scoped_refptr<GpuState>& gpu_state)
    : widget_(widget),
      gpu_state_(gpu_state),
      client_(client),
      client_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      next_transfer_buffer_id_(0),
      next_image_id_(0),
      next_fence_sync_release_(1),
      flushed_fence_sync_release_(0),
      sync_point_client_waiter_(
          gpu_state->sync_point_manager()->CreateSyncPointClientWaiter()),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

void CommandBufferLocal::Destroy() {
  DCHECK(CalledOnValidThread());
  weak_factory_.InvalidateWeakPtrs();
  // CommandBufferLocal is initialized on the GPU thread with
  // InitializeOnGpuThread(), so we need delete memebers on the GPU thread
  // too.
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferLocal::DeleteOnGpuThread,
                                base::Unretained(this)));
}

bool CommandBufferLocal::Initialize() {
  DCHECK(CalledOnValidThread());
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  base::WaitableEvent event(true, false);
  bool result = false;
  gpu_state_->command_buffer_task_runner()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CommandBufferLocal::InitializeOnGpuThread,
                 base::Unretained(this), base::Unretained(&event),
                 base::Unretained(&result)));
  event.Wait();
  return result;
}

gpu::CommandBuffer::State CommandBufferLocal::GetLastState() {
  DCHECK(CalledOnValidThread());
  return last_state_;
}

int32_t CommandBufferLocal::GetLastToken() {
  DCHECK(CalledOnValidThread());
  TryUpdateState();
  return last_state_.token;
}

void CommandBufferLocal::Flush(int32_t put_offset) {
  DCHECK(CalledOnValidThread());
  if (last_put_offset_ == put_offset)
    return;

  last_put_offset_ = put_offset;
  gpu::SyncPointManager* sync_point_manager = gpu_state_->sync_point_manager();
  const uint32_t order_num =
      driver_->sync_point_order_data()->GenerateUnprocessedOrderNumber(
          sync_point_manager);
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferLocal::FlushOnGpuThread,
                                base::Unretained(this), put_offset, order_num));
  flushed_fence_sync_release_ = next_fence_sync_release_ - 1;
}

void CommandBufferLocal::OrderingBarrier(int32_t put_offset) {
  DCHECK(CalledOnValidThread());
  // TODO(penghuang): Implement this more efficiently.
  Flush(put_offset);
}

void CommandBufferLocal::WaitForTokenInRange(int32_t start, int32_t end) {
  DCHECK(CalledOnValidThread());
  TryUpdateState();
  while (!InRange(start, end, last_state_.token) &&
         last_state_.error == gpu::error::kNoError) {
    MakeProgressAndUpdateState();
  }
}

void CommandBufferLocal::WaitForGetOffsetInRange(int32_t start, int32_t end) {
  DCHECK(CalledOnValidThread());
  TryUpdateState();
  while (!InRange(start, end, last_state_.get_offset) &&
         last_state_.error == gpu::error::kNoError) {
    MakeProgressAndUpdateState();
  }
}

void CommandBufferLocal::SetGetBuffer(int32_t buffer) {
  DCHECK(CalledOnValidThread());
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferLocal::SetGetBufferOnGpuThread,
                                base::Unretained(this), buffer));
  last_put_offset_ = -1;
}

scoped_refptr<gpu::Buffer> CommandBufferLocal::CreateTransferBuffer(
    size_t size,
    int32_t* id) {
  DCHECK(CalledOnValidThread());
  if (size >= std::numeric_limits<uint32_t>::max())
    return nullptr;

  void* memory = nullptr;
  mojo::ScopedSharedBufferHandle handle;
  mojo::ScopedSharedBufferHandle duped;
  if (!CreateMapAndDupSharedBuffer(size, &memory, &handle, &duped)) {
    if (last_state_.error == gpu::error::kNoError)
      last_state_.error = gpu::error::kLostContext;
    return nullptr;
  }

  *id = ++next_transfer_buffer_id_;

  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferLocal::RegisterTransferBufferOnGpuThread,
                 base::Unretained(this), *id, base::Passed(&duped),
                 static_cast<uint32_t>(size)));
  scoped_ptr<gpu::BufferBacking> backing(
      new mus::MojoBufferBacking(std::move(handle), memory, size));
  scoped_refptr<gpu::Buffer> buffer(new gpu::Buffer(std::move(backing)));
  return buffer;
}

void CommandBufferLocal::DestroyTransferBuffer(int32_t id) {
  DCHECK(CalledOnValidThread());
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferLocal::DestroyTransferBufferOnGpuThread,
                 base::Unretained(this), id));
}

gpu::Capabilities CommandBufferLocal::GetCapabilities() {
  DCHECK(CalledOnValidThread());
  return capabilities_;
}

int32_t CommandBufferLocal::CreateImage(ClientBuffer buffer,
                                        size_t width,
                                        size_t height,
                                        unsigned internal_format) {
  DCHECK(CalledOnValidThread());
  int32_t new_id = ++next_image_id_;
  mojo::SizePtr size = mojo::Size::New();
  size->width = static_cast<int32_t>(width);
  size->height = static_cast<int32_t>(height);

  mus::MojoGpuMemoryBufferImpl* gpu_memory_buffer =
      mus::MojoGpuMemoryBufferImpl::FromClientBuffer(buffer);
  gfx::GpuMemoryBufferHandle handle = gpu_memory_buffer->GetHandle();

  bool requires_sync_point = false;
  if (handle.type != gfx::SHARED_MEMORY_BUFFER) {
    requires_sync_point = true;
    NOTIMPLEMENTED();
    return -1;
  }

  base::SharedMemoryHandle dupd_handle =
      base::SharedMemory::DuplicateHandle(handle.handle);
#if defined(OS_WIN)
  HANDLE platform_handle = dupd_handle.GetHandle();
#else
  int platform_handle = dupd_handle.fd;
#endif

  MojoHandle mojo_handle = MOJO_HANDLE_INVALID;
  MojoResult create_result =
      MojoCreatePlatformHandleWrapper(platform_handle, &mojo_handle);
  // |MojoCreatePlatformHandleWrapper()| always takes the ownership of the
  // |platform_handle|, so we don't need close |platform_handle|.
  if (create_result != MOJO_RESULT_OK) {
    NOTIMPLEMENTED();
    return -1;
  }
  mojo::ScopedHandle scoped_handle;
  scoped_handle.reset(mojo::Handle(mojo_handle));

  const int32_t format = static_cast<int32_t>(gpu_memory_buffer->GetFormat());
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferLocal::CreateImageOnGpuThread,
                 base::Unretained(this), new_id, base::Passed(&scoped_handle),
                 handle.type, base::Passed(&size), format, internal_format));

  if (requires_sync_point) {
    NOTIMPLEMENTED();
    // TODO(jam): need to support this if we support types other than
    // SHARED_MEMORY_BUFFER.
    // gpu_memory_buffer_manager->SetDestructionSyncPoint(gpu_memory_buffer,
    // InsertSyncPoint());
  }

  return new_id;
}

void CommandBufferLocal::DestroyImage(int32_t id) {
  DCHECK(CalledOnValidThread());
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferLocal::DestroyImageOnGpuThread,
                                base::Unretained(this), id));
}

int32_t CommandBufferLocal::CreateGpuMemoryBufferImage(size_t width,
                                                       size_t height,
                                                       unsigned internal_format,
                                                       unsigned usage) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(usage, static_cast<unsigned>(GL_READ_WRITE_CHROMIUM));
  scoped_ptr<gfx::GpuMemoryBuffer> buffer(MojoGpuMemoryBufferImpl::Create(
      gfx::Size(static_cast<int>(width), static_cast<int>(height)),
      gpu::ImageFactory::DefaultBufferFormatForImageFormat(internal_format),
      gfx::BufferUsage::SCANOUT));
  if (!buffer)
    return -1;
  return CreateImage(buffer->AsClientBuffer(), width, height, internal_format);
}

void CommandBufferLocal::SignalQuery(uint32_t query_id,
                                     const base::Closure& callback) {
  DCHECK(CalledOnValidThread());

  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(), base::Bind(&CommandBufferLocal::SignalQueryOnGpuThread,
                                base::Unretained(this), query_id, callback));
}

void CommandBufferLocal::SetLock(base::Lock* lock) {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

bool CommandBufferLocal::IsGpuChannelLost() {
  DCHECK(CalledOnValidThread());
  // This is only possible for out-of-process command buffers.
  return false;
}

void CommandBufferLocal::EnsureWorkVisible() {
  // This is only relevant for out-of-process command buffers.
}

gpu::CommandBufferNamespace CommandBufferLocal::GetNamespaceID() const {
  DCHECK(CalledOnValidThread());
  return gpu::CommandBufferNamespace::MOJO_LOCAL;
}

uint64_t CommandBufferLocal::GetCommandBufferID() const {
  DCHECK(CalledOnValidThread());
  return driver_->GetCommandBufferID();
}

int32_t CommandBufferLocal::GetExtraCommandBufferData() const {
  DCHECK(CalledOnValidThread());
  return 0;
}

uint64_t CommandBufferLocal::GenerateFenceSyncRelease() {
  DCHECK(CalledOnValidThread());
  return next_fence_sync_release_++;
}

bool CommandBufferLocal::IsFenceSyncRelease(uint64_t release) {
  DCHECK(CalledOnValidThread());
  return release != 0 && release < next_fence_sync_release_;
}

bool CommandBufferLocal::IsFenceSyncFlushed(uint64_t release) {
  DCHECK(CalledOnValidThread());
  return release != 0 && release <= flushed_fence_sync_release_;
}

bool CommandBufferLocal::IsFenceSyncFlushReceived(uint64_t release) {
  DCHECK(CalledOnValidThread());
  return IsFenceSyncFlushed(release);
}

void CommandBufferLocal::SignalSyncToken(const gpu::SyncToken& sync_token,
                                         const base::Closure& callback) {
  DCHECK(CalledOnValidThread());
  scoped_refptr<gpu::SyncPointClientState> release_state =
      gpu_state_->sync_point_manager()->GetSyncPointClientState(
          sync_token.namespace_id(), sync_token.command_buffer_id());
  if (!release_state ||
      release_state->IsFenceSyncReleased(sync_token.release_count())) {
    callback.Run();
    return;
  }

  sync_point_client_waiter_->WaitOutOfOrderNonThreadSafe(
      release_state.get(), sync_token.release_count(),
      client_thread_task_runner_, callback);
}

bool CommandBufferLocal::CanWaitUnverifiedSyncToken(
    const gpu::SyncToken* sync_token) {
  DCHECK(CalledOnValidThread());
  // Right now, MOJO_LOCAL is only used by trusted code, so it is safe to wait
  // on a sync token in MOJO_LOCAL command buffer.
  return sync_token->namespace_id() == gpu::CommandBufferNamespace::MOJO_LOCAL;
}

void CommandBufferLocal::DidLoseContext(uint32_t reason) {
  if (client_) {
  driver_->set_client(nullptr);
    client_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CommandBufferLocal::DidLoseContextOnClientThread,
                   weak_ptr_, reason));
  }
}

void CommandBufferLocal::UpdateVSyncParameters(int64_t timebase,
                                               int64_t interval) {
  if (client_) {
    client_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&CommandBufferLocal::UpdateVSyncParametersOnClientThread,
                   weak_ptr_, timebase, interval));
  }
}

CommandBufferLocal::~CommandBufferLocal() {}

void CommandBufferLocal::TryUpdateState() {
  if (last_state_.error == gpu::error::kNoError)
    shared_state()->Read(&last_state_);
}

void CommandBufferLocal::MakeProgressAndUpdateState() {
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  base::WaitableEvent event(true, false);
  gpu::CommandBuffer::State state;
  gpu_state_->command_buffer_task_runner()->PostTask(
      driver_.get(),
      base::Bind(&CommandBufferLocal::MakeProgressOnGpuThread,
                 base::Unretained(this), base::Unretained(&event),
                 base::Unretained(&state)));
  event.Wait();
  if (state.generation - last_state_.generation < 0x80000000U)
    last_state_ = state;
}

void CommandBufferLocal::InitializeOnGpuThread(base::WaitableEvent* event,
                                               bool* result) {
  driver_.reset(new CommandBufferDriver(gpu::CommandBufferNamespace::MOJO_LOCAL,
                                        ++g_next_command_buffer_id, widget_,
                                        gpu_state_));
  const size_t kSharedStateSize = sizeof(gpu::CommandBufferSharedState);
  void* memory = nullptr;
  mojo::ScopedSharedBufferHandle duped;
  *result = CreateMapAndDupSharedBuffer(kSharedStateSize, &memory,
                                        &shared_state_handle_, &duped);

  if (!*result) {
    event->Signal();
    return;
  }

  shared_state_ = static_cast<gpu::CommandBufferSharedState*>(memory);
  shared_state()->Initialize();

  *result = driver_->Initialize(std::move(duped), mojo::Array<int32_t>::New(0));
  if (*result)
    capabilities_ = driver_->GetCapabilities();
  event->Signal();
}

bool CommandBufferLocal::FlushOnGpuThread(int32_t put_offset,
                                          uint32_t order_num) {
  DCHECK(driver_->IsScheduled());
  driver_->sync_point_order_data()->BeginProcessingOrderNumber(order_num);
  driver_->Flush(put_offset);

  // Return false if the Flush is not finished, so the CommandBufferTaskRunner
  // will not remove this task from the task queue.
  const bool complete = !driver_->HasUnprocessedCommands();
  if (complete)
    driver_->sync_point_order_data()->FinishProcessingOrderNumber(order_num);
  return complete;
}

bool CommandBufferLocal::SetGetBufferOnGpuThread(int32_t buffer) {
  DCHECK(driver_->IsScheduled());
  driver_->SetGetBuffer(buffer);
  return true;
}

bool CommandBufferLocal::RegisterTransferBufferOnGpuThread(
    int32_t id,
    mojo::ScopedSharedBufferHandle transfer_buffer,
    uint32_t size) {
  DCHECK(driver_->IsScheduled());
  driver_->RegisterTransferBuffer(id, std::move(transfer_buffer), size);
  return true;
}

bool CommandBufferLocal::DestroyTransferBufferOnGpuThread(int32_t id) {
  DCHECK(driver_->IsScheduled());
  driver_->DestroyTransferBuffer(id);
  return true;
}

bool CommandBufferLocal::CreateImageOnGpuThread(
    int32_t id,
    mojo::ScopedHandle memory_handle,
    int32_t type,
    mojo::SizePtr size,
    int32_t format,
    int32_t internal_format) {
  DCHECK(driver_->IsScheduled());
  driver_->CreateImage(id, std::move(memory_handle), type, std::move(size),
                       format, internal_format);
  return true;
}

bool CommandBufferLocal::DestroyImageOnGpuThread(int32_t id) {
  DCHECK(driver_->IsScheduled());
  driver_->DestroyImage(id);
  return true;
}

bool CommandBufferLocal::MakeProgressOnGpuThread(
    base::WaitableEvent* event,
    gpu::CommandBuffer::State* state) {
  DCHECK(driver_->IsScheduled());
  *state = driver_->GetLastState();
  event->Signal();
  return true;
}

bool CommandBufferLocal::DeleteOnGpuThread() {
  delete this;
  return true;
}

bool CommandBufferLocal::SignalQueryOnGpuThread(uint32_t query_id,
                                                const base::Closure& callback) {
  // |callback| should run on the client thread.
  driver_->SignalQuery(
      query_id, base::Bind(&PostTask, client_thread_task_runner_, callback));
  return true;
}

void CommandBufferLocal::DidLoseContextOnClientThread(uint32_t reason) {
  if (client_)
    client_->DidLoseContext();
}

void CommandBufferLocal::UpdateVSyncParametersOnClientThread(int64_t timebase,
                                                             int64_t interval) {
  if (client_)
    client_->UpdateVSyncParameters(timebase, interval);
}

}  // namespace mus
