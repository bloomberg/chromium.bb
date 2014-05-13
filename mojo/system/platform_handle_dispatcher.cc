// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/platform_handle_dispatcher.h"

#include "base/logging.h"

namespace mojo {
namespace system {

namespace {

const size_t kInvalidPlatformHandleIndex = static_cast<size_t>(-1);

struct SerializedPlatformHandleDispatcher {
  size_t platform_handle_index;  // (Or |kInvalidPlatformHandleIndex|.)
};

}  // namespace

PlatformHandleDispatcher::PlatformHandleDispatcher(
    embedder::ScopedPlatformHandle platform_handle)
    : platform_handle_(platform_handle.Pass()) {
}

embedder::ScopedPlatformHandle PlatformHandleDispatcher::PassPlatformHandle() {
  base::AutoLock locker(lock());
  return platform_handle_.Pass();
}

Dispatcher::Type PlatformHandleDispatcher::GetType() const {
  return kTypePlatformHandle;
}

PlatformHandleDispatcher::~PlatformHandleDispatcher() {
}

void PlatformHandleDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  platform_handle_.reset();
}

scoped_refptr<Dispatcher>
    PlatformHandleDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();
  return scoped_refptr<Dispatcher>(
      new PlatformHandleDispatcher(platform_handle_.Pass()));
}

void PlatformHandleDispatcher::StartSerializeImplNoLock(
    Channel* /*channel*/,
    size_t* max_size,
    size_t* max_platform_handles) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.
  *max_size = sizeof(SerializedPlatformHandleDispatcher);
  *max_platform_handles = 1;
}

bool PlatformHandleDispatcher::EndSerializeAndCloseImplNoLock(
    Channel* /*channel*/,
    void* destination,
    size_t* actual_size,
    std::vector<embedder::PlatformHandle>* platform_handles) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.

  SerializedPlatformHandleDispatcher* serialization =
      static_cast<SerializedPlatformHandleDispatcher*>(destination);
  if (platform_handle_.is_valid()) {
    serialization->platform_handle_index = platform_handles->size();
    platform_handles->push_back(platform_handle_.release());
  } else {
    serialization->platform_handle_index = kInvalidPlatformHandleIndex;
  }

  *actual_size = sizeof(SerializedPlatformHandleDispatcher);
  return true;
}

MojoWaitFlags PlatformHandleDispatcher::SatisfiedFlagsNoLock() const {
  return MOJO_WAIT_FLAG_NONE;
}

MojoWaitFlags PlatformHandleDispatcher::SatisfiableFlagsNoLock() const {
  return MOJO_WAIT_FLAG_NONE;
}

}  // namespace system
}  // namespace mojo
