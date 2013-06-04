// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/async_pixel_transfer_manager_idle.h"

#include <list>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
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

class AsyncPixelTransferStateImpl : public AsyncPixelTransferState {
 public:
  typedef base::Callback<void(GLuint)> TransferCallback;

  explicit AsyncPixelTransferStateImpl(GLuint texture_id)
      : id_(g_next_pixel_transfer_state_id++),
        texture_id_(texture_id),
        transfer_in_progress_(false) {
  }

  // Implement AsyncPixelTransferState:
  virtual bool TransferIsInProgress() OVERRIDE {
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
  virtual ~AsyncPixelTransferStateImpl() {}

  uint64 id_;
  GLuint texture_id_;
  bool transfer_in_progress_;
};

}  // namespace

// Class which handles async pixel transfers in a platform
// independent way.
class AsyncPixelTransferDelegateIdle : public AsyncPixelTransferDelegate,
    public base::SupportsWeakPtr<AsyncPixelTransferDelegateIdle> {
 public:
  AsyncPixelTransferDelegateIdle();
  virtual ~AsyncPixelTransferDelegateIdle();

  // implement AsyncPixelTransferDelegate:
  virtual AsyncPixelTransferState* CreatePixelTransferState(
      GLuint texture_id,
      const AsyncTexImage2DParams& define_params) OVERRIDE;
  virtual void BindCompletedAsyncTransfers() OVERRIDE;
  virtual void AsyncNotifyCompletion(
      const AsyncMemoryParams& mem_params,
      const CompletionCallback& callback) OVERRIDE;
  virtual void AsyncTexImage2D(
      AsyncPixelTransferState* transfer_state,
      const AsyncTexImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params,
      const base::Closure& bind_callback) OVERRIDE;
  virtual void AsyncTexSubImage2D(
      AsyncPixelTransferState* transfer_state,
      const AsyncTexSubImage2DParams& tex_params,
      const AsyncMemoryParams& mem_params) OVERRIDE;
  virtual void WaitForTransferCompletion(
      AsyncPixelTransferState* transfer_state) OVERRIDE;
  virtual uint32 GetTextureUploadCount() OVERRIDE;
  virtual base::TimeDelta GetTotalTextureUploadTime() OVERRIDE;
  virtual void ProcessMorePendingTransfers() OVERRIDE;
  virtual bool NeedsProcessMorePendingTransfers() OVERRIDE;

 private:
  struct Task {
    Task(uint64 transfer_id, const base::Closure& task);
    ~Task();

    // This is non-zero if pixel transfer task.
    uint64 transfer_id;

    base::Closure task;
  };

  void ProcessNotificationTasks();

  void PerformNotifyCompletion(
      AsyncMemoryParams mem_params,
      ScopedSafeSharedMemory* safe_shared_memory,
      const CompletionCallback& callback);
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

  int texture_upload_count_;
  base::TimeDelta total_texture_upload_time_;

  std::list<Task> tasks_;

  DISALLOW_COPY_AND_ASSIGN(AsyncPixelTransferDelegateIdle);
};

AsyncPixelTransferDelegateIdle::Task::Task(
    uint64 transfer_id, const base::Closure& task)
    : transfer_id(transfer_id),
      task(task) {
}

AsyncPixelTransferDelegateIdle::Task::~Task() {}

AsyncPixelTransferDelegateIdle::AsyncPixelTransferDelegateIdle()
    : texture_upload_count_(0) {
}

AsyncPixelTransferDelegateIdle::~AsyncPixelTransferDelegateIdle() {}

AsyncPixelTransferState* AsyncPixelTransferDelegateIdle::
    CreatePixelTransferState(GLuint texture_id,
                             const AsyncTexImage2DParams& define_params) {
  return new AsyncPixelTransferStateImpl(texture_id);
}

void AsyncPixelTransferDelegateIdle::BindCompletedAsyncTransfers() {
  // Everything is already bound.
}

void AsyncPixelTransferDelegateIdle::AsyncNotifyCompletion(
    const AsyncMemoryParams& mem_params,
    const CompletionCallback& callback) {
  if (tasks_.empty()) {
    callback.Run(mem_params);
    return;
  }

  tasks_.push_back(
      Task(0,  // 0 transfer_id for notification tasks.
           base::Bind(
               &AsyncPixelTransferDelegateIdle::PerformNotifyCompletion,
               AsWeakPtr(),
               mem_params,
               base::Owned(new ScopedSafeSharedMemory(
                               safe_shared_memory_pool(),
                               mem_params.shared_memory,
                               mem_params.shm_size)),
               callback)));
}

void AsyncPixelTransferDelegateIdle::AsyncTexImage2D(
    AsyncPixelTransferState* transfer_state,
    const AsyncTexImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params,
    const base::Closure& bind_callback) {
  AsyncPixelTransferStateImpl* state =
      static_cast<AsyncPixelTransferStateImpl*>(transfer_state);
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), tex_params.target);
  DCHECK(mem_params.shared_memory);
  DCHECK_LE(mem_params.shm_data_offset + mem_params.shm_data_size,
            mem_params.shm_size);
  DCHECK(state);

  tasks_.push_back(
      Task(state->id(),
           base::Bind(
               &AsyncPixelTransferStateImpl::PerformTransfer,
               base::AsWeakPtr(state),
               base::Bind(
                   &AsyncPixelTransferDelegateIdle::PerformAsyncTexImage2D,
                   AsWeakPtr(),
                   tex_params,
                   mem_params,
                   bind_callback,
                   base::Owned(new ScopedSafeSharedMemory(
                                   safe_shared_memory_pool(),
                                   mem_params.shared_memory,
                                   mem_params.shm_size))))));

  state->set_transfer_in_progress(true);
}

void AsyncPixelTransferDelegateIdle::AsyncTexSubImage2D(
    AsyncPixelTransferState* transfer_state,
    const AsyncTexSubImage2DParams& tex_params,
    const AsyncMemoryParams& mem_params) {
  AsyncPixelTransferStateImpl* state =
      static_cast<AsyncPixelTransferStateImpl*>(transfer_state);
  DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D), tex_params.target);
  DCHECK(mem_params.shared_memory);
  DCHECK_LE(mem_params.shm_data_offset + mem_params.shm_data_size,
            mem_params.shm_size);
  DCHECK(state);

  tasks_.push_back(
      Task(state->id(),
           base::Bind(
               &AsyncPixelTransferStateImpl::PerformTransfer,
               base::AsWeakPtr(state),
               base::Bind(
                   &AsyncPixelTransferDelegateIdle::PerformAsyncTexSubImage2D,
                   AsWeakPtr(),
                   tex_params,
                   mem_params,
                   base::Owned(new ScopedSafeSharedMemory(
                                   safe_shared_memory_pool(),
                                   mem_params.shared_memory,
                                   mem_params.shm_size))))));

  state->set_transfer_in_progress(true);
}

void AsyncPixelTransferDelegateIdle::WaitForTransferCompletion(
    AsyncPixelTransferState* transfer_state) {
  AsyncPixelTransferStateImpl* state =
      static_cast<AsyncPixelTransferStateImpl*>(transfer_state);
  DCHECK(state);

  for (std::list<Task>::iterator iter = tasks_.begin();
       iter != tasks_.end(); ++iter) {
    if (iter->transfer_id != state->id())
      continue;

    (*iter).task.Run();
    tasks_.erase(iter);
    break;
  }

  ProcessNotificationTasks();
}

uint32 AsyncPixelTransferDelegateIdle::GetTextureUploadCount() {
  return texture_upload_count_;
}

base::TimeDelta AsyncPixelTransferDelegateIdle::GetTotalTextureUploadTime() {
  return total_texture_upload_time_;
}

void AsyncPixelTransferDelegateIdle::ProcessMorePendingTransfers() {
  if (tasks_.empty())
    return;

  // First task should always be a pixel transfer task.
  DCHECK(tasks_.front().transfer_id);
  tasks_.front().task.Run();
  tasks_.pop_front();

  ProcessNotificationTasks();
}

bool AsyncPixelTransferDelegateIdle::NeedsProcessMorePendingTransfers() {
  return !tasks_.empty();
}

void AsyncPixelTransferDelegateIdle::ProcessNotificationTasks() {
  while (!tasks_.empty()) {
    // Stop when we reach a pixel transfer task.
    if (tasks_.front().transfer_id)
      return;

    tasks_.front().task.Run();
    tasks_.pop_front();
  }
}

void AsyncPixelTransferDelegateIdle::PerformNotifyCompletion(
    AsyncMemoryParams mem_params,
    ScopedSafeSharedMemory* safe_shared_memory,
    const CompletionCallback& callback) {
  TRACE_EVENT0("gpu", "PerformNotifyCompletion");
  AsyncMemoryParams safe_mem_params = mem_params;
  safe_mem_params.shared_memory = safe_shared_memory->shared_memory();
  callback.Run(safe_mem_params);
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

  texture_upload_count_++;
  total_texture_upload_time_ += base::TimeTicks::HighResNow() - begin_time;
}

AsyncPixelTransferManagerIdle::AsyncPixelTransferManagerIdle()
    : delegate_(new AsyncPixelTransferDelegateIdle()) {}

AsyncPixelTransferManagerIdle::~AsyncPixelTransferManagerIdle() {}

AsyncPixelTransferDelegate*
AsyncPixelTransferManagerIdle::GetAsyncPixelTransferDelegate() {
  return delegate_.get();
}

}  // namespace gpu
