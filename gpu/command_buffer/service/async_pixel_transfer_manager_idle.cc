// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/async_pixel_transfer_manager_idle.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/service/safe_shared_memory_pool.h"
#include "ui/gl/scoped_binders.h"

namespace gpu {

namespace {

base::LazyInstance<SafeSharedMemoryPool> g_safe_shared_memory_pool =
    LAZY_INSTANCE_INITIALIZER;

SafeSharedMemoryPool* safe_shared_memory_pool() {
  return g_safe_shared_memory_pool.Pointer();
}

static uint64 g_next_pixel_transfer_state_id = 1;

void PerformNotifyCompletion(
    AsyncMemoryParams mem_params,
    ScopedSafeSharedMemory* safe_shared_memory,
    const AsyncPixelTransferManager::CompletionCallback& callback) {
  TRACE_EVENT0("gpu", "PerformNotifyCompletion");
  AsyncMemoryParams safe_mem_params = mem_params;
  safe_mem_params.shared_memory = safe_shared_memory->shared_memory();
  callback.Run(safe_mem_params);
}

// TODO(backer): Merge this with Delegate in follow-up CL. It serves no purpose.
class AsyncPixelTransferState
    : public base::SupportsWeakPtr<AsyncPixelTransferState> {
 public:
  typedef base::Callback<void(GLuint)> TransferCallback;

  explicit AsyncPixelTransferState(GLuint texture_id)
      : id_(g_next_pixel_transfer_state_id++),
        texture_id_(texture_id),
        transfer_in_progress_(false) {
  }

  virtual ~AsyncPixelTransferState() {}

  bool TransferIsInProgress()  {
    return transfer_in_progress_;
  }

  uint64 id() const { return id_; }

  void set_transfer_in_progress(bool transfer_in_progress) {
    transfer_in_progress_ = transfer_in_progress;
  }

  void PerformTransfer(const TransferCallback& callback) {
    DCHECK(texture_id_);
    DCHECK(transfer_in_progress_);
    callback.Run(texture_id_);
    transfer_in_progress_ = false;
  }

 private:
  uint64 id_;
  GLuint texture_id_;
  bool transfer_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferState);
};

}  // namespace

// Class which handles async pixel transfers in a platform
// independent way.
class AsyncPixelTransferDelegateIdle : public AsyncPixelTransferDelegate,
    public base::SupportsWeakPtr<AsyncPixelTransferDelegateIdle> {
 public:
  AsyncPixelTransferDelegateIdle(
      AsyncPixelTransferManagerIdle::SharedState* state,
      GLuint texture_id);
  virtual ~AsyncPixelTransferDelegateIdle();

  // Implement AsyncPixelTransferDelegate:
  virtual void AsyncTexImage2D(
      const AsyncTexImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params,
      const base::Closure& bind_callback) OVERRIDE;
  virtual void AsyncTexSubImage2D(
      const AsyncTexSubImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params) OVERRIDE;
  virtual bool TransferIsInProgress() OVERRIDE;
  virtual void WaitForTransferCompletion() OVERRIDE;

 private:
  void PerformAsyncTexImage2D(
      AsyncTexImage2DParams tex_params,
      AsyncMemoryParams mem_params,
      const base::Closure& bind_callback,
      ScopedSafeSharedMemory* safe_shared_memory,
      GLuint texture_id);
  void PerformAsyncTexSubImage2D(
      AsyncTexSubImage2DParams tex_params,
      AsyncMemoryParams mem_params,
      ScopedSafeSharedMemory* safe_shared_memory,
      GLuint texture_id);

  // Safe to hold a raw pointer because SharedState is owned by the Manager
  // which owns the Delegate.
  AsyncPixelTransferManagerIdle::SharedState* shared_state_;
  scoped_ptr<AsyncPixelTransferState> state_;

  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferDelegateIdle);
};

AsyncPixelTransferDelegateIdle::AsyncPixelTransferDelegateIdle(
    AsyncPixelTransferManagerIdle::SharedState* shared_state,
    GLuint texture_id)
    : shared_state_(shared_state),
      state_(new AsyncPixelTransferState(texture_id)) {}

AsyncPixelTransferDelegateIdle::~AsyncPixelTransferDelegateIdle() {}

void AsyncPixelTransferDelegateIdle::AsyncTexImage2D(
    const AsyncTexImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params,
    const base::Closure& bind_callback) {
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), tex_params.target);
  DCHECK(mem_params.shared_memory);
  DCHECK_LE(mem_params.shm_data_offset + mem_params.shm_data_size,
            mem_params.shm_size);

  shared_state_->tasks.push_back(AsyncPixelTransferManagerIdle::Task(
      state_->id(),
      base::Bind(
          &AsyncPixelTransferState::PerformTransfer,
          state_->AsWeakPtr(),
          base::Bind(
              &AsyncPixelTransferDelegateIdle::PerformAsyncTexImage2D,
              AsWeakPtr(),
              tex_params,
              mem_params,
              bind_callback,
              base::Owned(new ScopedSafeSharedMemory(safe_shared_memory_pool(),
                                                     mem_params.shared_memory,
                                                     mem_params.shm_size))))));

  state_->set_transfer_in_progress(true);
}

void AsyncPixelTransferDelegateIdle::AsyncTexSubImage2D(
    const AsyncTexSubImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params) {
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), tex_params.target);
  DCHECK(mem_params.shared_memory);
  DCHECK_LE(mem_params.shm_data_offset + mem_params.shm_data_size,
            mem_params.shm_size);

  shared_state_->tasks.push_back(AsyncPixelTransferManagerIdle::Task(
      state_->id(),
      base::Bind(
          &AsyncPixelTransferState::PerformTransfer,
          state_->AsWeakPtr(),
          base::Bind(
              &AsyncPixelTransferDelegateIdle::PerformAsyncTexSubImage2D,
              AsWeakPtr(),
              tex_params,
              mem_params,
              base::Owned(new ScopedSafeSharedMemory(safe_shared_memory_pool(),
                                                     mem_params.shared_memory,
                                                     mem_params.shm_size))))));

  state_->set_transfer_in_progress(true);
}

bool  AsyncPixelTransferDelegateIdle::TransferIsInProgress() {
  return state_->TransferIsInProgress();
}

void AsyncPixelTransferDelegateIdle::WaitForTransferCompletion() {
  for (std::list<AsyncPixelTransferManagerIdle::Task>::iterator iter =
           shared_state_->tasks.begin();
       iter != shared_state_->tasks.end();
       ++iter) {
    if (iter->transfer_id != state_->id())
      continue;

    (*iter).task.Run();
    shared_state_->tasks.erase(iter);
    break;
  }

  shared_state_->ProcessNotificationTasks();
}

void AsyncPixelTransferDelegateIdle::PerformAsyncTexImage2D(
    AsyncTexImage2DParams tex_params,
    AsyncMemoryParams mem_params,
    const base::Closure& bind_callback,
    ScopedSafeSharedMemory* safe_shared_memory,
    GLuint texture_id) {
  TRACE_EVENT2("gpu", "PerformAsyncTexImage2D",
               "width", tex_params.width,
               "height", tex_params.height);

  void* data = GetAddress(safe_shared_memory, mem_params);

  gfx::ScopedTextureBinder texture_binder(tex_params.target, texture_id);

  {
    TRACE_EVENT0("gpu", "glTexImage2D");
    glTexImage2D(
        tex_params.target,
        tex_params.level,
        tex_params.internal_format,
        tex_params.width,
        tex_params.height,
        tex_params.border,
        tex_params.format,
        tex_params.type,
        data);
  }

  // The texture is already fully bound so just call it now.
  bind_callback.Run();
}

void AsyncPixelTransferDelegateIdle::PerformAsyncTexSubImage2D(
    AsyncTexSubImage2DParams tex_params,
    AsyncMemoryParams mem_params,
    ScopedSafeSharedMemory* safe_shared_memory,
    GLuint texture_id) {
  TRACE_EVENT2("gpu", "PerformAsyncTexSubImage2D",
               "width", tex_params.width,
               "height", tex_params.height);

  void* data = GetAddress(safe_shared_memory, mem_params);

  base::TimeTicks begin_time(base::TimeTicks::HighResNow());
  gfx::ScopedTextureBinder texture_binder(tex_params.target, texture_id);

  {
    TRACE_EVENT0("gpu", "glTexSubImage2D");
    glTexSubImage2D(
        tex_params.target,
        tex_params.level,
        tex_params.xoffset,
        tex_params.yoffset,
        tex_params.width,
        tex_params.height,
        tex_params.format,
        tex_params.type,
        data);
  }

  shared_state_->texture_upload_count++;
  shared_state_->total_texture_upload_time +=
      base::TimeTicks::HighResNow() - begin_time;
}

AsyncPixelTransferManagerIdle::Task::Task(
    uint64 transfer_id, const base::Closure& task)
    : transfer_id(transfer_id),
      task(task) {
}

AsyncPixelTransferManagerIdle::Task::~Task() {}

AsyncPixelTransferManagerIdle::SharedState::SharedState()
    : texture_upload_count(0) {}

AsyncPixelTransferManagerIdle::SharedState::~SharedState() {}

void AsyncPixelTransferManagerIdle::SharedState::ProcessNotificationTasks() {
  while (!tasks.empty()) {
    // Stop when we reach a pixel transfer task.
    if (tasks.front().transfer_id)
      return;

    tasks.front().task.Run();
    tasks.pop_front();
  }
}

AsyncPixelTransferManagerIdle::AsyncPixelTransferManagerIdle()
  : shared_state_() {
}

AsyncPixelTransferManagerIdle::~AsyncPixelTransferManagerIdle() {}

void AsyncPixelTransferManagerIdle::BindCompletedAsyncTransfers() {
  // Everything is already bound.
}

void AsyncPixelTransferManagerIdle::AsyncNotifyCompletion(
    const AsyncMemoryParams& mem_params,
    const CompletionCallback& callback) {
  if (shared_state_.tasks.empty()) {
    callback.Run(mem_params);
    return;
  }

  shared_state_.tasks.push_back(
      Task(0,  // 0 transfer_id for notification tasks.
           base::Bind(
               &PerformNotifyCompletion,
               mem_params,
               base::Owned(new ScopedSafeSharedMemory(safe_shared_memory_pool(),
                                                      mem_params.shared_memory,
                                                      mem_params.shm_size)),
               callback)));
}

uint32 AsyncPixelTransferManagerIdle::GetTextureUploadCount() {
  return shared_state_.texture_upload_count;
}

base::TimeDelta AsyncPixelTransferManagerIdle::GetTotalTextureUploadTime() {
  return shared_state_.total_texture_upload_time;
}

void AsyncPixelTransferManagerIdle::ProcessMorePendingTransfers() {
  if (shared_state_.tasks.empty())
    return;

  // First task should always be a pixel transfer task.
  DCHECK(shared_state_.tasks.front().transfer_id);
  shared_state_.tasks.front().task.Run();
  shared_state_.tasks.pop_front();

  shared_state_.ProcessNotificationTasks();
}

bool AsyncPixelTransferManagerIdle::NeedsProcessMorePendingTransfers() {
  return !shared_state_.tasks.empty();
}

AsyncPixelTransferDelegate*
AsyncPixelTransferManagerIdle::CreatePixelTransferDelegateImpl(
    gles2::TextureRef* ref,
    const AsyncTexImage2DParams& define_params) {
  return new AsyncPixelTransferDelegateIdle(&shared_state_, ref->service_id());
}

}  // namespace gpu
