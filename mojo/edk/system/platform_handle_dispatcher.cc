// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/platform_handle_dispatcher.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <utility>

#include "base/logging.h"

namespace mojo {
namespace edk {

namespace {

const uint32_t kInvalidPlatformHandleIndex = static_cast<uint32_t>(-1);

struct MOJO_ALIGNAS(8) SerializedPlatformHandleDispatcher {
  MOJO_ALIGNAS(4) uint32_t
      platform_handle_index;  // (Or |kInvalidPlatformHandleIndex|.)
};

}  // namespace

ScopedPlatformHandle PlatformHandleDispatcher::PassPlatformHandle() {
  base::AutoLock locker(lock());
  return std::move(platform_handle_);
}

Dispatcher::Type PlatformHandleDispatcher::GetType() const {
  return Type::PLATFORM_HANDLE;
}

// static
scoped_refptr<PlatformHandleDispatcher> PlatformHandleDispatcher::Deserialize(
    const void* source,
    size_t size,
    PlatformHandleVector* platform_handles) {
  if (size != sizeof(SerializedPlatformHandleDispatcher)) {
    LOG(ERROR) << "Invalid serialized platform handle dispatcher (bad size)";
    return nullptr;
  }

  const SerializedPlatformHandleDispatcher* serialization =
      static_cast<const SerializedPlatformHandleDispatcher*>(source);
  size_t platform_handle_index = serialization->platform_handle_index;

  // Starts off invalid, which is what we want.
  PlatformHandle platform_handle;

  if (platform_handle_index != kInvalidPlatformHandleIndex) {
    if (!platform_handles ||
        platform_handle_index >= platform_handles->size()) {
      LOG(ERROR)
          << "Invalid serialized platform handle dispatcher (missing handles)";
      return nullptr;
    }

    // We take ownership of the handle, so we have to invalidate the one in
    // |platform_handles|.
    std::swap(platform_handle, (*platform_handles)[platform_handle_index]);
  }

  return Create(ScopedPlatformHandle(platform_handle));
}

PlatformHandleDispatcher::PlatformHandleDispatcher(
    ScopedPlatformHandle platform_handle)
    : platform_handle_(std::move(platform_handle)) {}

PlatformHandleDispatcher::~PlatformHandleDispatcher() {
}

void PlatformHandleDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  platform_handle_.reset();
}

scoped_refptr<Dispatcher>
PlatformHandleDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();
  return Create(std::move(platform_handle_));
}

void PlatformHandleDispatcher::StartSerializeImplNoLock(
    size_t* max_size,
    size_t* max_platform_handles) {
  *max_size = sizeof(SerializedPlatformHandleDispatcher);
  *max_platform_handles = 1;
}

bool PlatformHandleDispatcher::EndSerializeAndCloseImplNoLock(
    void* destination,
    size_t* actual_size,
    PlatformHandleVector* platform_handles) {
  SerializedPlatformHandleDispatcher* serialization =
      static_cast<SerializedPlatformHandleDispatcher*>(destination);
  if (platform_handle_.is_valid()) {
    DCHECK(platform_handles->size() < std::numeric_limits<uint32_t>::max());
    serialization->platform_handle_index =
        static_cast<uint32_t>(platform_handles->size());
    platform_handles->push_back(platform_handle_.release());
  } else {
    serialization->platform_handle_index = kInvalidPlatformHandleIndex;
  }

  *actual_size = sizeof(SerializedPlatformHandleDispatcher);
  return true;
}

}  // namespace edk
}  // namespace mojo
