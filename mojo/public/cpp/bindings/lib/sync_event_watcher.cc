// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/sync_event_watcher.h"

#include "base/logging.h"

namespace mojo {

SyncEventWatcher::SyncEventWatcher(base::WaitableEvent* event,
                                   const base::Closure& callback)
    : event_(event),
      callback_(callback),
      registry_(SyncHandleRegistry::current()),
      destroyed_(new base::RefCountedData<bool>(false)) {}

SyncEventWatcher::~SyncEventWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (registered_)
    registry_->UnregisterEvent(event_);
  destroyed_->data = true;
}

void SyncEventWatcher::AllowWokenUpBySyncWatchOnSameThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IncrementRegisterCount();
}

bool SyncEventWatcher::SyncWatch(const bool* should_stop) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IncrementRegisterCount();
  if (!registered_) {
    DecrementRegisterCount();
    return false;
  }

  // This object may be destroyed during the Wait() call. So we have to preserve
  // the boolean that Wait uses.
  auto destroyed = destroyed_;
  const bool* should_stop_array[] = {should_stop, &destroyed->data};
  bool result = registry_->Wait(should_stop_array, 2);

  // This object has been destroyed.
  if (destroyed->data)
    return false;

  DecrementRegisterCount();
  return result;
}

void SyncEventWatcher::IncrementRegisterCount() {
  register_request_count_++;
  if (!registered_)
    registered_ = registry_->RegisterEvent(event_, callback_);
}

void SyncEventWatcher::DecrementRegisterCount() {
  DCHECK_GT(register_request_count_, 0u);
  register_request_count_--;
  if (register_request_count_ == 0 && registered_) {
    registry_->UnregisterEvent(event_);
    registered_ = false;
  }
}

}  // namespace mojo
