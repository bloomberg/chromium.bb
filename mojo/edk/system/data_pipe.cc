// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/data_pipe.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <limits>

#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/options_validation.h"
#include "mojo/edk/system/raw_channel.h"
#include "mojo/edk/system/transport_data.h"

namespace mojo {
namespace edk {

namespace {

const uint32_t kInvalidDataPipeHandleIndex = static_cast<uint32_t>(-1);

struct MOJO_ALIGNAS(8) SerializedDataPipeHandleDispatcher {
  MOJO_ALIGNAS(4)
  uint32_t platform_handle_index;  // (Or |kInvalidDataPipeHandleIndex|.)

  // These are from MojoCreateDataPipeOptions
  MOJO_ALIGNAS(4) MojoCreateDataPipeOptionsFlags flags;
  MOJO_ALIGNAS(4) uint32_t element_num_bytes;
  MOJO_ALIGNAS(4) uint32_t capacity_num_bytes;

  MOJO_ALIGNAS(4)
  uint32_t shared_memory_handle_index;  // (Or |kInvalidDataPipeHandleIndex|.)
  MOJO_ALIGNAS(4) uint32_t shared_memory_size;
};

}  // namespace

MojoCreateDataPipeOptions DataPipe::GetDefaultCreateOptions() {
  MojoCreateDataPipeOptions result = {
      static_cast<uint32_t>(sizeof(MojoCreateDataPipeOptions)),
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,
      1u,
      static_cast<uint32_t>(
          GetConfiguration().default_data_pipe_capacity_bytes)};
  return result;
}

MojoResult DataPipe::ValidateCreateOptions(
    const MojoCreateDataPipeOptions* in_options,
    MojoCreateDataPipeOptions* out_options) {
  const MojoCreateDataPipeOptionsFlags kKnownFlags =
      MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;

  *out_options = GetDefaultCreateOptions();
  if (!in_options)
    return MOJO_RESULT_OK;

  UserOptionsReader<MojoCreateDataPipeOptions> reader(in_options);
  if (!reader.is_valid())
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (!OPTIONS_STRUCT_HAS_MEMBER(MojoCreateDataPipeOptions, flags, reader))
    return MOJO_RESULT_OK;
  if ((reader.options().flags & ~kKnownFlags))
    return MOJO_RESULT_UNIMPLEMENTED;
  out_options->flags = reader.options().flags;

  // Checks for fields beyond |flags|:

  if (!OPTIONS_STRUCT_HAS_MEMBER(MojoCreateDataPipeOptions, element_num_bytes,
                                 reader))
    return MOJO_RESULT_OK;
  if (reader.options().element_num_bytes == 0)
    return MOJO_RESULT_INVALID_ARGUMENT;
  out_options->element_num_bytes = reader.options().element_num_bytes;

  if (!OPTIONS_STRUCT_HAS_MEMBER(MojoCreateDataPipeOptions, capacity_num_bytes,
                                 reader) ||
      reader.options().capacity_num_bytes == 0) {
    // Round the default capacity down to a multiple of the element size (but at
    // least one element).
    size_t default_data_pipe_capacity_bytes =
        GetConfiguration().default_data_pipe_capacity_bytes;
    out_options->capacity_num_bytes =
        std::max(static_cast<uint32_t>(default_data_pipe_capacity_bytes -
                                       (default_data_pipe_capacity_bytes %
                                        out_options->element_num_bytes)),
                 out_options->element_num_bytes);
    return MOJO_RESULT_OK;
  }
  if (reader.options().capacity_num_bytes % out_options->element_num_bytes != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (reader.options().capacity_num_bytes >
      GetConfiguration().max_data_pipe_capacity_bytes)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  out_options->capacity_num_bytes = reader.options().capacity_num_bytes;

  return MOJO_RESULT_OK;
}

void DataPipe::StartSerialize(bool have_channel_handle,
                              bool have_shared_memory,
                              size_t* max_size,
                              size_t* max_platform_handles) {
  *max_size = sizeof(SerializedDataPipeHandleDispatcher);
  *max_platform_handles = 0;
  if (have_channel_handle)
    (*max_platform_handles)++;
  if (have_shared_memory)
    (*max_platform_handles)++;
}

void DataPipe::EndSerialize(const MojoCreateDataPipeOptions& options,
                            ScopedPlatformHandle channel_handle,
                            ScopedPlatformHandle shared_memory_handle,
                            size_t shared_memory_size,
                            void* destination,
                            size_t* actual_size,
                            PlatformHandleVector* platform_handles) {
  SerializedDataPipeHandleDispatcher* serialization =
      static_cast<SerializedDataPipeHandleDispatcher*>(destination);
  if (channel_handle.is_valid()) {
    DCHECK(platform_handles->size() < std::numeric_limits<uint32_t>::max());
    serialization->platform_handle_index =
        static_cast<uint32_t>(platform_handles->size());
    platform_handles->push_back(channel_handle.release());
  } else {
    serialization->platform_handle_index = kInvalidDataPipeHandleIndex;
  }

  serialization->flags = options.flags;
  serialization->element_num_bytes = options.element_num_bytes;
  serialization->capacity_num_bytes = options.capacity_num_bytes;

  serialization->shared_memory_size = static_cast<uint32_t>(shared_memory_size);
  if (serialization->shared_memory_size) {
    DCHECK(platform_handles->size() < std::numeric_limits<uint32_t>::max());
    serialization->shared_memory_handle_index =
        static_cast<uint32_t>(platform_handles->size());
    platform_handles->push_back(shared_memory_handle.release());
  } else {
    serialization->shared_memory_handle_index = kInvalidDataPipeHandleIndex;
  }

  *actual_size = sizeof(SerializedDataPipeHandleDispatcher);
}

ScopedPlatformHandle DataPipe::Deserialize(
    const void* source,
    size_t size,
    PlatformHandleVector* platform_handles,
    MojoCreateDataPipeOptions* options,
    ScopedPlatformHandle* shared_memory_handle,
    size_t* shared_memory_size) {
  if (size != sizeof(SerializedDataPipeHandleDispatcher)) {
    LOG(ERROR) << "Invalid serialized data pipe dispatcher (bad size)";
    return ScopedPlatformHandle();
  }

  const SerializedDataPipeHandleDispatcher* serialization =
      static_cast<const SerializedDataPipeHandleDispatcher*>(source);
  size_t platform_handle_index = serialization->platform_handle_index;

  // Starts off invalid, which is what we want.
  PlatformHandle platform_handle;
  if (platform_handle_index != kInvalidDataPipeHandleIndex) {
    if (!platform_handles ||
        platform_handle_index >= platform_handles->size()) {
      LOG(ERROR)
          << "Invalid serialized data pipe dispatcher (missing handles)";
      return ScopedPlatformHandle();
    }

    // We take ownership of the handle, so we have to invalidate the one in
    // |platform_handles|.
    std::swap(platform_handle, (*platform_handles)[platform_handle_index]);
  }

  options->struct_size = sizeof(MojoCreateDataPipeOptions);
  options->flags = serialization->flags;
  options->element_num_bytes = serialization->element_num_bytes;
  options->capacity_num_bytes = serialization->capacity_num_bytes;

  if (shared_memory_size) {
    *shared_memory_size = serialization->shared_memory_size;
    if (*shared_memory_size) {
      DCHECK(serialization->shared_memory_handle_index !=
             kInvalidDataPipeHandleIndex);
      if (!platform_handles ||
          serialization->shared_memory_handle_index >=
              platform_handles->size()) {
        LOG(ERROR) << "Invalid serialized data pipe dispatcher "
                   << "(missing handles)";
        return ScopedPlatformHandle();
      }

      PlatformHandle temp_shared_memory_handle;
      std::swap(temp_shared_memory_handle,
                (*platform_handles)[serialization->shared_memory_handle_index]);
      *shared_memory_handle =
          ScopedPlatformHandle(temp_shared_memory_handle);
    }
  }

  size -= sizeof(SerializedDataPipeHandleDispatcher);

  return ScopedPlatformHandle(platform_handle);
}

}  // namespace edk
}  // namespace mojo
