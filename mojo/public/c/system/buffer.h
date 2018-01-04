// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains types/constants and functions specific to shared buffers.
//
// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_BUFFER_H_
#define MOJO_PUBLIC_C_SYSTEM_BUFFER_H_

#include <stdint.h>

#include "mojo/public/c/system/macros.h"
#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

// |MojoCreateSharedBufferOptions|: Used to specify creation parameters for a
// shared buffer to |MojoCreateSharedBuffer()|.
//
//   |uint32_t struct_size|: Set to the size of the
//       |MojoCreateSharedBufferOptions| struct. (Used to allow for future
//       extensions.)
//
//   |MojoCreateSharedBufferOptionsFlags flags|: Reserved for future use.
//       |MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE|: No flags; default mode.

typedef uint32_t MojoCreateSharedBufferOptionsFlags;

#ifdef __cplusplus
const MojoCreateSharedBufferOptionsFlags
    MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE = 0;
#else
#define MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE \
  ((MojoCreateSharedBufferOptionsFlags)0)
#endif

MOJO_STATIC_ASSERT(MOJO_ALIGNOF(int64_t) == 8, "int64_t has weird alignment");
struct MOJO_ALIGNAS(8) MojoCreateSharedBufferOptions {
  uint32_t struct_size;
  MojoCreateSharedBufferOptionsFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(MojoCreateSharedBufferOptions) == 8,
                   "MojoCreateSharedBufferOptions has wrong size");

// |MojoDuplicateBufferHandleOptions|: Used to specify parameters in duplicating
// access to a shared buffer to |MojoDuplicateBufferHandle()|.
//
//   |uint32_t struct_size|: Set to the size of the
//       |MojoDuplicateBufferHandleOptions| struct. (Used to allow for future
//       extensions.)
//
//   |MojoDuplicateBufferHandleOptionsFlags flags|: Flags to control the
//       behavior of |MojoDuplicateBufferHandle()|. May be some combination of
//       the following:
//
//       |MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE|: No flags; default
//           mode.
//       |MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_READ_ONLY|: The duplicate
//           shared buffer can only be mapped read-only. A read-only duplicate
//           may only be created before any handles to the buffer are passed
//           over a message pipe.

typedef uint32_t MojoDuplicateBufferHandleOptionsFlags;

#ifdef __cplusplus
const MojoDuplicateBufferHandleOptionsFlags
    MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE = 0;
const MojoDuplicateBufferHandleOptionsFlags
    MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_READ_ONLY = 1 << 0;
#else
#define MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE \
  ((MojoDuplicateBufferHandleOptionsFlags)0)
#define MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_READ_ONLY \
  ((MojoDuplicateBufferHandleOptionsFlags)1 << 0)
#endif

struct MojoDuplicateBufferHandleOptions {
  uint32_t struct_size;
  MojoDuplicateBufferHandleOptionsFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(MojoDuplicateBufferHandleOptions) == 8,
                   "MojoDuplicateBufferHandleOptions has wrong size");

// |MojoMapBufferFlags|: Used to specify different modes to |MojoMapBuffer()|.
//   |MOJO_MAP_BUFFER_FLAG_NONE| - No flags; default mode.

typedef uint32_t MojoMapBufferFlags;

#ifdef __cplusplus
const MojoMapBufferFlags MOJO_MAP_BUFFER_FLAG_NONE = 0;
#else
#define MOJO_MAP_BUFFER_FLAG_NONE ((MojoMapBufferFlags)0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Note: See the comment in functions.h about the meaning of the "optional"
// label for pointer parameters.

// Creates a buffer of size |num_bytes| bytes that can be shared between
// processes. The returned handle may be duplicated any number of times by
// |MojoDuplicateBufferHandle()|.
//
// To access the buffer's storage, one must call |MojoMapBuffer()|.
//
// |options| may be set to null for a shared buffer with the default options.
//
// On success, |*shared_buffer_handle| will be set to the handle for the shared
// buffer. On failure it is not modified.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |*options| is invalid).
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if a process/system/quota/etc. limit has
//       been reached (e.g., if the requested size was too large, or if the
//       maximum number of handles was exceeded).
//   |MOJO_RESULT_UNIMPLEMENTED| if an unsupported flag was set in |*options|.
MOJO_SYSTEM_EXPORT MojoResult MojoCreateSharedBuffer(
    const struct MojoCreateSharedBufferOptions* options,  // Optional.
    uint64_t num_bytes,                                   // In.
    MojoHandle* shared_buffer_handle);                    // Out.

// Duplicates the handle |buffer_handle| as a new shared buffer handle. On
// success this returns the new handle in |*new_buffer_handle|. A shared buffer
// remains allocated as long as there is at least one shared buffer handle
// referencing it in at least one process in the system.
//
// |options| may be set to null to duplicate the buffer handle with the default
// options.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |buffer_handle| is not a valid buffer handle or |*options| is invalid).
//   |MOJO_RESULT_UNIMPLEMENTED| if an unsupported flag was set in |*options|.
MOJO_SYSTEM_EXPORT MojoResult MojoDuplicateBufferHandle(
    MojoHandle buffer_handle,
    const struct MojoDuplicateBufferHandleOptions* options,  // Optional.
    MojoHandle* new_buffer_handle);                          // Out.

// Maps the part (at offset |offset| of length |num_bytes|) of the buffer given
// by |buffer_handle| into memory, with options specified by |flags|. |offset +
// num_bytes| must be less than or equal to the size of the buffer. On success,
// |*buffer| points to memory with the requested part of the buffer. On
// failure |*buffer| it is not modified.
//
// A single buffer handle may have multiple active mappings. The permissions
// (e.g., writable or executable) of the returned memory depend on the
// properties of the buffer and properties attached to the buffer handle, as
// well as |flags|.
//
// A mapped buffer must eventually be unmapped by calling |MojoUnmapBuffer()|
// with the value of |*buffer| returned by this function.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |buffer_handle| is not a valid buffer handle or the range specified by
//       |offset| and |num_bytes| is not valid).
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if the mapping operation itself failed
//       (e.g., due to not having appropriate address space available).
MOJO_SYSTEM_EXPORT MojoResult MojoMapBuffer(MojoHandle buffer_handle,
                                            uint64_t offset,
                                            uint64_t num_bytes,
                                            void** buffer,  // Out.
                                            MojoMapBufferFlags flags);

// Unmaps a buffer pointer that was mapped by |MojoMapBuffer()|. |buffer| must
// have been the result of |MojoMapBuffer()| (not some other pointer inside
// the mapped memory), and the entire mapping will be removed.
//
// A mapping may only be unmapped once.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |buffer| is invalid (e.g., is not the
//       result of |MojoMapBuffer()| or has already been unmapped).
MOJO_SYSTEM_EXPORT MojoResult MojoUnmapBuffer(void* buffer);  // In.

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_BUFFER_H_
