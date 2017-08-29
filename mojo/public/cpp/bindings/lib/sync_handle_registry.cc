// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/sync_handle_registry.h"

#include <algorithm>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/c/system/core.h"

namespace mojo {
namespace {

base::LazyInstance<
    base::SequenceLocalStorageSlot<scoped_refptr<SyncHandleRegistry>>>::Leaky
    g_current_sync_handle_watcher = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
scoped_refptr<SyncHandleRegistry> SyncHandleRegistry::current() {
  // SyncMessageFilter can be used on threads without sequence-local storage
  // being available. Those receive a unique, standalone SyncHandleRegistry.
  if (!base::SequencedTaskRunnerHandle::IsSet())
    return new SyncHandleRegistry();

  scoped_refptr<SyncHandleRegistry> result =
      g_current_sync_handle_watcher.Get().Get();
  if (!result) {
    result = new SyncHandleRegistry();
    g_current_sync_handle_watcher.Get().Set(result);
  }
  return result;
}

bool SyncHandleRegistry::RegisterHandle(const Handle& handle,
                                        MojoHandleSignals handle_signals,
                                        const HandleCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (base::ContainsKey(handles_, handle))
    return false;

  MojoResult result = wait_set_.AddHandle(handle, handle_signals);
  if (result != MOJO_RESULT_OK)
    return false;

  handles_[handle] = callback;
  return true;
}

void SyncHandleRegistry::UnregisterHandle(const Handle& handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::ContainsKey(handles_, handle))
    return;

  MojoResult result = wait_set_.RemoveHandle(handle);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  handles_.erase(handle);
}

void SyncHandleRegistry::RegisterEvent(base::WaitableEvent* event,
                                       const base::Closure& callback) {
  auto it = events_.find(event);
  if (it == events_.end()) {
    auto result = events_.emplace(event, base::StackVector<base::Closure, 1>{});
    it = result.first;
  }

  auto& callbacks = it->second.container();
  if (callbacks.empty()) {
    // AddEvent() must succeed since we only attempt it when there are
    // previously no callbacks registered for this event.
    MojoResult rv = wait_set_.AddEvent(event);
    DCHECK_EQ(MOJO_RESULT_OK, rv);
  }

  callbacks.push_back(callback);
}

void SyncHandleRegistry::UnregisterEvent(base::WaitableEvent* event,
                                         const base::Closure& callback) {
  auto it = events_.find(event);
  if (it == events_.end())
    return;

  auto& callbacks = it->second.container();
  callbacks.erase(std::remove_if(callbacks.begin(), callbacks.end(),
                                 [&callback](const base::Closure& cb) {
                                   return cb.Equals(callback);
                                 }),
                  callbacks.end());

  if (callbacks.empty()) {
    events_.erase(it);
    MojoResult rv = wait_set_.RemoveEvent(event);
    DCHECK_EQ(MOJO_RESULT_OK, rv);
  }
}

bool SyncHandleRegistry::Wait(const bool* should_stop[], size_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t num_ready_handles;
  Handle ready_handle;
  MojoResult ready_handle_result;

  scoped_refptr<SyncHandleRegistry> preserver(this);
  while (true) {
    for (size_t i = 0; i < count; ++i)
      if (*should_stop[i])
        return true;

    // TODO(yzshen): Theoretically it can reduce sync call re-entrancy if we
    // give priority to the handle that is waiting for sync response.
    base::WaitableEvent* ready_event = nullptr;
    num_ready_handles = 1;
    wait_set_.Wait(&ready_event, &num_ready_handles, &ready_handle,
                   &ready_handle_result);
    if (num_ready_handles) {
      DCHECK_EQ(1u, num_ready_handles);
      const auto iter = handles_.find(ready_handle);
      iter->second.Run(ready_handle_result);
    }

    if (ready_event) {
      const auto iter = events_.find(ready_event);
      DCHECK(iter != events_.end());
      for (auto& callback : iter->second.container())
        callback.Run();
    }
  };

  return false;
}

SyncHandleRegistry::SyncHandleRegistry() = default;

SyncHandleRegistry::~SyncHandleRegistry() = default;

}  // namespace mojo
