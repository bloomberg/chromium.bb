// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/weak_handle.h"

#include <sstream>

#include "base/message_loop_proxy.h"
#include "base/tracked.h"

namespace browser_sync {

namespace internal {

WeakHandleCoreBase::WeakHandleCoreBase()
    : owner_loop_(MessageLoop::current()),
      owner_loop_proxy_(base::MessageLoopProxy::current()),
      destroyed_on_owner_thread_(false) {
  owner_loop_->AddDestructionObserver(this);
}

bool WeakHandleCoreBase::IsOnOwnerThread() const {
  // We can't use |owner_loop_proxy_->BelongsToCurrentThread()| as
  // it may not work from within a MessageLoop::DestructionObserver
  // callback.
  return MessageLoop::current() == owner_loop_;
}

void WeakHandleCoreBase::WillDestroyCurrentMessageLoop() {
  CHECK(IsOnOwnerThread());
  CHECK(!destroyed_on_owner_thread_);
  // NOTE: This function dispatches to
  // WeakHandle<T>::CleanupOnOwnerThread() (i.e., not just
  // WeakHandleCoreBase::CleanupOnOwnerThread() is run).
  CleanupOnOwnerThread();
  CHECK(destroyed_on_owner_thread_);
}

WeakHandleCoreBase::~WeakHandleCoreBase() {
  // It is safe to read |destroyed_on_owner_thread_| here even if
  // we're not on the owner thread (see comments on
  // base::AtomicRefCountDecN()).
  CHECK(destroyed_on_owner_thread_);
}

void WeakHandleCoreBase::CleanupOnOwnerThread() {
  CHECK(IsOnOwnerThread());
  CHECK(!destroyed_on_owner_thread_);
  owner_loop_->RemoveDestructionObserver(this);
  destroyed_on_owner_thread_ = true;
}

namespace {

// TODO(akalin): Merge with similar function in
// js_transaction_observer.cc.
std::string GetLocationString(const tracked_objects::Location& location) {
  std::ostringstream oss;
  oss << location.function_name() << "@"
      << location.file_name() << ":" << location.line_number();
  return oss.str();
}

}  // namespace

void WeakHandleCoreBase::PostToOwnerThread(
    const tracked_objects::Location& from_here,
    const base::Closure& fn) const {
  if (!owner_loop_proxy_->PostTask(from_here, fn)) {
    VLOG(1) << "Could not post task from " << GetLocationString(from_here);
  }
}

void WeakHandleCoreBase::Destroy() {
  if (IsOnOwnerThread()) {
    CHECK(!destroyed_on_owner_thread_);
    CleanupAndDestroyOnOwnerThread();
  } else if (!owner_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&WeakHandleCoreBase::CleanupAndDestroyOnOwnerThread,
                 base::Unretained(this)))) {
    // If the post fails, that means that the owner loop is gone and
    // therefore CleanupOnOwnerThread() should have already been
    // called via WillDestroyCurrentMessageLoop().  Therefore, the
    // only thing left is to self-destruct.
    delete this;
  }
}

void WeakHandleCoreBase::CleanupAndDestroyOnOwnerThread() {
  CHECK(IsOnOwnerThread());
  CHECK(!destroyed_on_owner_thread_);
  CleanupOnOwnerThread();
  CHECK(destroyed_on_owner_thread_);
  delete this;
}

void WeakHandleCoreBaseTraits::Destruct(const WeakHandleCoreBase* core_base) {
  const_cast<WeakHandleCoreBase*>(core_base)->Destroy();
}

}  // namespace internal

}  // namespace base
