// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/shared_buffer_dispatcher.h"

#include <limits>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/c/system/macros.h"
#include "mojo/system/constants.h"
#include "mojo/system/memory.h"
#include "mojo/system/options_validation.h"
#include "mojo/system/raw_shared_buffer.h"

namespace mojo {
namespace system {

namespace {

struct SerializedSharedBufferDispatcher {
  size_t num_bytes;
  size_t platform_handle_index;
};

}  // namespace

// static
MojoResult SharedBufferDispatcher::ValidateOptions(
    const MojoCreateSharedBufferOptions* in_options,
    MojoCreateSharedBufferOptions* out_options) {
  static const MojoCreateSharedBufferOptions kDefaultOptions = {
    static_cast<uint32_t>(sizeof(MojoCreateSharedBufferOptions)),
    MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE
  };
  *out_options = kDefaultOptions;

  if (!in_options)
    return MOJO_RESULT_OK;

  if (!IsOptionsStructPointerAndSizeValid<MojoCreateSharedBufferOptions>(
          in_options))
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (!HAS_OPTIONS_STRUCT_MEMBER(MojoCreateSharedBufferOptions, flags,
                                 in_options))
    return MOJO_RESULT_OK;
  if (!AreOptionsFlagsAllKnown<MojoCreateSharedBufferOptions>(
          in_options, MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE))
    return MOJO_RESULT_UNIMPLEMENTED;
  out_options->flags = in_options->flags;

  return MOJO_RESULT_OK;
}

// static
MojoResult SharedBufferDispatcher::Create(
    const MojoCreateSharedBufferOptions& /*validated_options*/,
    uint64_t num_bytes,
    scoped_refptr<SharedBufferDispatcher>* result) {
  if (!num_bytes)
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_bytes > kMaxSharedMemoryNumBytes)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  scoped_refptr<RawSharedBuffer> shared_buffer(
      RawSharedBuffer::Create(static_cast<size_t>(num_bytes)));
  if (!shared_buffer)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  *result = new SharedBufferDispatcher(shared_buffer);
  return MOJO_RESULT_OK;
}

Dispatcher::Type SharedBufferDispatcher::GetType() const {
  return kTypeSharedBuffer;
}

// static
scoped_refptr<SharedBufferDispatcher> SharedBufferDispatcher::Deserialize(
    Channel* channel,
    const void* source,
    size_t size,
    embedder::PlatformHandleVector* platform_handles) {
  if (size != sizeof(SerializedSharedBufferDispatcher)) {
    LOG(ERROR) << "Invalid serialized shared buffer dispatcher (bad size)";
    return scoped_refptr<SharedBufferDispatcher>();
  }

  const SerializedSharedBufferDispatcher* serialization =
      static_cast<const SerializedSharedBufferDispatcher*>(source);
  size_t num_bytes = serialization->num_bytes;
  size_t platform_handle_index = serialization->platform_handle_index;

  if (!num_bytes) {
    LOG(ERROR)
        << "Invalid serialized shared buffer dispatcher (invalid num_bytes)";
    return scoped_refptr<SharedBufferDispatcher>();
  }

  if (!platform_handles || platform_handle_index >= platform_handles->size()) {
    LOG(ERROR)
        << "Invalid serialized shared buffer dispatcher (missing handles)";
    return scoped_refptr<SharedBufferDispatcher>();
  }

  // Starts off invalid, which is what we want.
  embedder::PlatformHandle platform_handle;
  // We take ownership of the handle, so we have to invalidate the one in
  // |platform_handles|.
  std::swap(platform_handle, (*platform_handles)[platform_handle_index]);

  // Wrapping |platform_handle| in a |ScopedPlatformHandle| means that it'll be
  // closed even if creation fails.
  scoped_refptr<RawSharedBuffer> shared_buffer(
      RawSharedBuffer::CreateFromPlatformHandle(num_bytes,
      embedder::ScopedPlatformHandle(platform_handle)));
  if (!shared_buffer) {
    LOG(ERROR)
        << "Invalid serialized shared buffer dispatcher (invalid num_bytes?)";
    return scoped_refptr<SharedBufferDispatcher>();
  }

  return scoped_refptr<SharedBufferDispatcher>(new SharedBufferDispatcher(
      shared_buffer));
}

SharedBufferDispatcher::SharedBufferDispatcher(
    scoped_refptr<RawSharedBuffer> shared_buffer)
    : shared_buffer_(shared_buffer) {
  DCHECK(shared_buffer_);
}

SharedBufferDispatcher::~SharedBufferDispatcher() {
}

void SharedBufferDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  DCHECK(shared_buffer_);
  shared_buffer_ = NULL;
}

scoped_refptr<Dispatcher>
    SharedBufferDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();
  DCHECK(shared_buffer_);
  scoped_refptr<RawSharedBuffer> shared_buffer;
  shared_buffer.swap(shared_buffer_);
  return scoped_refptr<Dispatcher>(new SharedBufferDispatcher(shared_buffer));
}

MojoResult SharedBufferDispatcher::DuplicateBufferHandleImplNoLock(
    const MojoDuplicateBufferHandleOptions* options,
    scoped_refptr<Dispatcher>* new_dispatcher) {
  lock().AssertAcquired();
  if (options) {
    // The |struct_size| field must be valid to read.
    if (!VerifyUserPointer<uint32_t>(&options->struct_size))
      return MOJO_RESULT_INVALID_ARGUMENT;
    // And then |options| must point to at least |options->struct_size| bytes.
    if (!VerifyUserPointerWithSize<MOJO_ALIGNOF(int64_t)>(options,
                                                          options->struct_size))
      return MOJO_RESULT_INVALID_ARGUMENT;

    if (options->struct_size < sizeof(*options))
      return MOJO_RESULT_INVALID_ARGUMENT;
    // We don't actually do anything with |options|.
  }

  *new_dispatcher = new SharedBufferDispatcher(shared_buffer_);
  return MOJO_RESULT_OK;
}

MojoResult SharedBufferDispatcher::MapBufferImplNoLock(
    uint64_t offset,
    uint64_t num_bytes,
    MojoMapBufferFlags flags,
    scoped_ptr<RawSharedBufferMapping>* mapping) {
  lock().AssertAcquired();
  DCHECK(shared_buffer_);

  if (offset > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_bytes > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (!shared_buffer_->IsValidMap(static_cast<size_t>(offset),
                                  static_cast<size_t>(num_bytes)))
    return MOJO_RESULT_INVALID_ARGUMENT;

  DCHECK(mapping);
  *mapping = shared_buffer_->MapNoCheck(static_cast<size_t>(offset),
                                        static_cast<size_t>(num_bytes));
  if (!*mapping)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  return MOJO_RESULT_OK;
}

void SharedBufferDispatcher::StartSerializeImplNoLock(
    Channel* /*channel*/,
    size_t* max_size,
    size_t* max_platform_handles) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.
  *max_size = sizeof(SerializedSharedBufferDispatcher);
  *max_platform_handles = 1;
}

bool SharedBufferDispatcher::EndSerializeAndCloseImplNoLock(
    Channel* /*channel*/,
    void* destination,
    size_t* actual_size,
    embedder::PlatformHandleVector* platform_handles) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.
  DCHECK(shared_buffer_);

  SerializedSharedBufferDispatcher* serialization =
      static_cast<SerializedSharedBufferDispatcher*>(destination);
  // If there's only one reference to |shared_buffer_|, then it's ours (and no
  // one else can make any more references to it), so we can just take its
  // handle.
  embedder::ScopedPlatformHandle platform_handle(
      shared_buffer_->HasOneRef() ?
          shared_buffer_->PassPlatformHandle() :
          shared_buffer_->DuplicatePlatformHandle());
  if (!platform_handle.is_valid()) {
    shared_buffer_ = NULL;
    return false;
  }

  serialization->num_bytes = shared_buffer_->num_bytes();
  serialization->platform_handle_index = platform_handles->size();
  platform_handles->push_back(platform_handle.release());
  *actual_size = sizeof(SerializedSharedBufferDispatcher);

  shared_buffer_ = NULL;

  return true;
}

MojoWaitFlags SharedBufferDispatcher::SatisfiedFlagsNoLock() const {
  // TODO(vtl): Add transferrable flag.
  return MOJO_WAIT_FLAG_NONE;
}

MojoWaitFlags SharedBufferDispatcher::SatisfiableFlagsNoLock() const {
  // TODO(vtl): Add transferrable flag.
  return MOJO_WAIT_FLAG_NONE;
}

}  // namespace system
}  // namespace mojo
