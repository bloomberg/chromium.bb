// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/sync_handle_registry.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "mojo/public/c/system/core.h"

namespace mojo {
namespace internal {
namespace {

base::LazyInstance<base::ThreadLocalPointer<SyncHandleRegistry>>
    g_current_sync_handle_watcher = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
SyncHandleRegistry* SyncHandleRegistry::current() {
  SyncHandleRegistry* result = g_current_sync_handle_watcher.Pointer()->Get();
  if (!result) {
    result = new SyncHandleRegistry();
    DCHECK_EQ(result, g_current_sync_handle_watcher.Pointer()->Get());
  }
  return result;
}

bool SyncHandleRegistry::RegisterHandle(const Handle& handle,
                                        MojoHandleSignals handle_signals,
                                        const HandleCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (ContainsKey(handles_, handle))
    return false;

  MojoResult result = MojoAddHandle(wait_set_handle_.get().value(),
                                    handle.value(), handle_signals);
  if (result != MOJO_RESULT_OK)
    return false;

  handles_[handle] = callback;
  return true;
}

void SyncHandleRegistry::UnregisterHandle(const Handle& handle) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!ContainsKey(handles_, handle))
    return;

  MojoResult result =
      MojoRemoveHandle(wait_set_handle_.get().value(), handle.value());
  DCHECK_EQ(MOJO_RESULT_OK, result);
  handles_.erase(handle);
}

bool SyncHandleRegistry::WatchAllHandles(const bool* should_stop[],
                                         size_t count) {
  DCHECK(thread_checker_.CalledOnValidThread());

  MojoResult result;
  uint32_t num_ready_handles;
  MojoHandle ready_handle;
  MojoResult ready_handle_result;

  // This object may be destroyed during a callback. So we have to preserve
  // the boolean.
  scoped_refptr<base::RefCountedData<bool>> destroyed = destroyed_;
  while (!destroyed->data) {
    for (size_t i = 0; i < count; ++i)
      if (*should_stop[i])
        return true;
    do {
      result = Wait(wait_set_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                    MOJO_DEADLINE_INDEFINITE, nullptr);
      if (result != MOJO_RESULT_OK)
        return false;

      // TODO(yzshen): Theoretically it can reduce sync call re-entrancy if we
      // give priority to the handle that is waiting for sync response.
      num_ready_handles = 1;
      result = MojoGetReadyHandles(wait_set_handle_.get().value(),
                                   &num_ready_handles, &ready_handle,
                                   &ready_handle_result, nullptr);
      if (result != MOJO_RESULT_OK && result != MOJO_RESULT_SHOULD_WAIT)
        return false;
    } while (result == MOJO_RESULT_SHOULD_WAIT);

    const auto iter = handles_.find(Handle(ready_handle));
    iter->second.Run(ready_handle_result);
  };

  return false;
}

SyncHandleRegistry::SyncHandleRegistry()
    : destroyed_(new base::RefCountedData<bool>(false)) {
  MojoHandle handle;
  MojoResult result = MojoCreateWaitSet(&handle);
  CHECK_EQ(MOJO_RESULT_OK, result);
  wait_set_handle_.reset(Handle(handle));
  CHECK(wait_set_handle_.is_valid());

  DCHECK(!g_current_sync_handle_watcher.Pointer()->Get());
  g_current_sync_handle_watcher.Pointer()->Set(this);

  base::MessageLoop::current()->AddDestructionObserver(this);
}

SyncHandleRegistry::~SyncHandleRegistry() {
  DCHECK(thread_checker_.CalledOnValidThread());
  destroyed_->data = true;
  g_current_sync_handle_watcher.Pointer()->Set(nullptr);
}

void SyncHandleRegistry::WillDestroyCurrentMessageLoop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(this, g_current_sync_handle_watcher.Pointer()->Get());

  base::MessageLoop::current()->RemoveDestructionObserver(this);

  delete this;
}

}  // namespace internal
}  // namespace mojo
