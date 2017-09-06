// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains types/constants and functions specific to data pipes.
//
// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_DATA_PIPE_H_
#define MOJO_PUBLIC_C_SYSTEM_DATA_PIPE_H_

#include <stdint.h>

#include "mojo/public/c/system/macros.h"
#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

// |MojoCreateDataPipeOptions|: Used to specify creation parameters for a data
// pipe to |MojoCreateDataPipe()|.
//
//   |uint32_t struct_size|: Set to the size of the |MojoCreateDataPipeOptions|
//       struct. (Used to allow for future extensions.)
//
//   |MojoCreateDataPipeOptionsFlags flags|: Used to specify different modes of
//       operation. May be some combination of the following values:
//
//       |MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE|: No flags; default mode.
//
//   |uint32_t element_num_bytes|: The size of an element, in bytes. All
//       transactions and buffers will consist of an integral number of
//       elements. Must be nonzero.
//
//   |uint32_t capacity_num_bytes|: The capacity of the data pipe, in number of
//       bytes; must be a multiple of |element_num_bytes|. The data pipe will
//       always be able to queue AT LEAST this much data. Set to zero to opt for
//       a system-dependent automatically-calculated capacity (which will always
//       be at least one element).

typedef uint32_t MojoCreateDataPipeOptionsFlags;

#ifdef __cplusplus
const MojoCreateDataPipeOptionsFlags MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE =
    0;
#else
#define MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE \
  ((MojoCreateDataPipeOptionsFlags)0)
#endif

MOJO_STATIC_ASSERT(MOJO_ALIGNOF(int64_t) == 8, "int64_t has weird alignment");
struct MOJO_ALIGNAS(8) MojoCreateDataPipeOptions {
  MOJO_ALIGNAS(4) uint32_t struct_size;
  MOJO_ALIGNAS(4) MojoCreateDataPipeOptionsFlags flags;
  MOJO_ALIGNAS(4) uint32_t element_num_bytes;
  MOJO_ALIGNAS(4) uint32_t capacity_num_bytes;
};
MOJO_STATIC_ASSERT(sizeof(MojoCreateDataPipeOptions) == 16,
                   "MojoCreateDataPipeOptions has wrong size");

// |MojoWriteDataFlags|: Used to specify different modes to |MojoWriteData()|
// and |MojoBeginWriteData()|. May be some combination of the following values:
//
//   |MOJO_WRITE_DATA_FLAG_NONE| - No flags; default mode.
//   |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| - Write either all the elements
//      requested or none of them.

typedef uint32_t MojoWriteDataFlags;

#ifdef __cplusplus
const MojoWriteDataFlags MOJO_WRITE_DATA_FLAG_NONE = 0;
const MojoWriteDataFlags MOJO_WRITE_DATA_FLAG_ALL_OR_NONE = 1 << 0;
#else
#define MOJO_WRITE_DATA_FLAG_NONE ((MojoWriteDataFlags)0)
#define MOJO_WRITE_DATA_FLAG_ALL_OR_NONE ((MojoWriteDataFlags)1 << 0)
#endif

// |MojoReadDataFlags|: Used to specify different modes to |MojoReadData()| and
// |MojoBeginReadData()|. May be some combination of the following values:
//
//   |MOJO_READ_DATA_FLAG_NONE| - No flags; default mode.
//   |MOJO_READ_DATA_FLAG_ALL_OR_NONE| - Read (or discard) either the requested
//        number of elements or none. For use with |MojoReadData()| only.
//   |MOJO_READ_DATA_FLAG_DISCARD| - Discard (up to) the requested number of
//        elements. For use with |MojoReadData()| only.
//   |MOJO_READ_DATA_FLAG_QUERY| - Query the number of elements available to
//       read. For use with |MojoReadData()| only. Mutually exclusive with
//       |MOJO_READ_DATA_FLAG_DISCARD|, and |MOJO_READ_DATA_FLAG_ALL_OR_NONE|
//       is ignored if this flag is set.
//   |MOJO_READ_DATA_FLAG_PEEK| - Read elements without removing them. For use
//       with |MojoReadData()| only. Mutually exclusive with
//       |MOJO_READ_DATA_FLAG_DISCARD| and |MOJO_READ_DATA_FLAG_QUERY|.

typedef uint32_t MojoReadDataFlags;

#ifdef __cplusplus
const MojoReadDataFlags MOJO_READ_DATA_FLAG_NONE = 0;
const MojoReadDataFlags MOJO_READ_DATA_FLAG_ALL_OR_NONE = 1 << 0;
const MojoReadDataFlags MOJO_READ_DATA_FLAG_DISCARD = 1 << 1;
const MojoReadDataFlags MOJO_READ_DATA_FLAG_QUERY = 1 << 2;
const MojoReadDataFlags MOJO_READ_DATA_FLAG_PEEK = 1 << 3;
#else
#define MOJO_READ_DATA_FLAG_NONE ((MojoReadDataFlags)0)
#define MOJO_READ_DATA_FLAG_ALL_OR_NONE ((MojoReadDataFlags)1 << 0)
#define MOJO_READ_DATA_FLAG_DISCARD ((MojoReadDataFlags)1 << 1)
#define MOJO_READ_DATA_FLAG_QUERY ((MojoReadDataFlags)1 << 2)
#define MOJO_READ_DATA_FLAG_PEEK ((MojoReadDataFlags)1 << 3)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Note: See the comment in functions.h about the meaning of the "optional"
// label for pointer parameters.

// Creates a data pipe, which is a unidirectional communication channel for
// unframed data. Data must be read and written in multiples of discrete
// discrete elements of size given in |options|.
//
// See |MojoCreateDataPipeOptions| for a description of the different options
// available for data pipes.
//
// |options| may be set to null for a data pipe with the default options (which
// will have an element size of one byte and have some system-dependent
// capacity).
//
// On success, |*data_pipe_producer_handle| will be set to the handle for the
// producer and |*data_pipe_consumer_handle| will be set to the handle for the
// consumer. On failure they are not modified.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid, e.g.,
//       |*options| is invalid, specified capacity or element size is zero, or
//       the specified element size exceeds the specified capacity.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if a process/system/quota/etc. limit has
//       been reached (e.g., if the requested capacity was too large, or if the
//       maximum number of handles was exceeded).
//   |MOJO_RESULT_UNIMPLEMENTED| if an unsupported flag was set in |*options|.
MOJO_SYSTEM_EXPORT MojoResult MojoCreateDataPipe(
    const struct MojoCreateDataPipeOptions* options,  // Optional.
    MojoHandle* data_pipe_producer_handle,            // Out.
    MojoHandle* data_pipe_consumer_handle);           // Out.

// Writes the data pipe producer given by |data_pipe_producer_handle|.
//
// |elements| points to data of size |*num_bytes|; |*num_bytes| must be a
// multiple of the data pipe's element size. If
// |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| is set in |flags|, either all the data
// is written (if enough write capacity is available) or none is.
//
// On success |*num_bytes| is set to the amount of data that was actually
// written. On failure it is unmodified.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |data_pipe_producer_dispatcher| is not a handle to a data pipe
//       producer or |*num_bytes| is not a multiple of the data pipe's element
//       size.)
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe consumer handle has been
//       closed.
//   |MOJO_RESULT_OUT_OF_RANGE| if |flags| has
//       |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| set and the required amount of data
//       (specified by |*num_bytes|) could not be written.
//   |MOJO_RESULT_BUSY| if there is a two-phase write ongoing with
//       |data_pipe_producer_handle| (i.e., |MojoBeginWriteData()| has been
//       called, but not yet the matching |MojoEndWriteData()|).
//   |MOJO_RESULT_SHOULD_WAIT| if no data can currently be written (and the
//       consumer is still open) and |flags| does *not* have
//       |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| set.
MOJO_SYSTEM_EXPORT MojoResult
    MojoWriteData(MojoHandle data_pipe_producer_handle,
                  const void* elements,
                  uint32_t* num_bytes,  // In/out.
                  MojoWriteDataFlags flags);

// Begins a two-phase write to the data pipe producer given by
// |data_pipe_producer_handle|. On success |*buffer| will be a pointer to which
// the caller can write up to |*buffer_num_bytes| bytes of data.
//
// During a two-phase write, |data_pipe_producer_handle| is *not* writable.
// If another caller tries to write to it by calling |MojoWriteData()| or
// |MojoBeginWriteData()|, their request will fail with |MOJO_RESULT_BUSY|.
//
// If |MojoBeginWriteData()| returns MOJO_RESULT_OK and once the caller has
// finished writing data to |*buffer|, |MojoEndWriteData()| must be called to
// indicate the amount of data actually written and to complete the two-phase
// write operation. |MojoEndWriteData()| need not be called when
// |MojoBeginWriteData()| fails.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |data_pipe_producer_handle| is not a handle to a data pipe producer or
//       flags has |MOJO_WRITE_DATA_FLAG_ALL_OR_NONE| set.
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe consumer handle has been
//       closed.
//   |MOJO_RESULT_BUSY| if there is already a two-phase write ongoing with
//       |data_pipe_producer_handle| (i.e., |MojoBeginWriteData()| has been
//       called, but not yet the matching |MojoEndWriteData()|).
//   |MOJO_RESULT_SHOULD_WAIT| if no data can currently be written (and the
//       consumer is still open).
MOJO_SYSTEM_EXPORT MojoResult
    MojoBeginWriteData(MojoHandle data_pipe_producer_handle,
                       void** buffer,               // Out.
                       uint32_t* buffer_num_bytes,  // In/out.
                       MojoWriteDataFlags flags);

// Ends a two-phase write that was previously initiated by
// |MojoBeginWriteData()| for the same |data_pipe_producer_handle|.
//
// |num_bytes_written| must indicate the number of bytes actually written into
// the two-phase write buffer. It must be less than or equal to the value of
// |*buffer_num_bytes| output by |MojoBeginWriteData()|, and it must be a
// multiple of the data pipe's element size.
//
// On failure, the two-phase write (if any) is ended (so the handle may become
// writable again if there's space available) but no data written to |*buffer|
// is "put into" the data pipe.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |data_pipe_producer_handle| is not a handle to a data pipe producer or
//       |num_bytes_written| is invalid (greater than the maximum value provided
//       by |MojoBeginWriteData()| or not a multiple of the element size).
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe producer is not in a
//       two-phase write (e.g., |MojoBeginWriteData()| was not called or
//       |MojoEndWriteData()| has already been called).
MOJO_SYSTEM_EXPORT MojoResult
    MojoEndWriteData(MojoHandle data_pipe_producer_handle,
                     uint32_t num_bytes_written);

// Reads data from the data pipe consumer given by |data_pipe_consumer_handle|.
// May also be used to discard data or query the amount of data available.
//
// If |flags| has neither |MOJO_READ_DATA_FLAG_DISCARD| nor
// |MOJO_READ_DATA_FLAG_QUERY| set, this tries to read up to |*num_bytes| (which
// must be a multiple of the data pipe's element size) bytes of data to
// |elements| and set |*num_bytes| to the amount actually read. If flags has
// |MOJO_READ_DATA_FLAG_ALL_OR_NONE| set, it will either read exactly
// |*num_bytes| bytes of data or none. Additionally, if flags has
// |MOJO_READ_DATA_FLAG_PEEK| set, the data read will remain in the pipe and be
// available to future reads.
//
// If flags has |MOJO_READ_DATA_FLAG_DISCARD| set, it discards up to
// |*num_bytes| (which again must be a multiple of the element size) bytes of
// data, setting |*num_bytes| to the amount actually discarded. If flags has
// |MOJO_READ_DATA_FLAG_ALL_OR_NONE|, it will either discard exactly
// |*num_bytes| bytes of data or none. In this case, |MOJO_READ_DATA_FLAG_QUERY|
// must not be set, and |elements| is ignored (and should typically be set to
// null).
//
// If flags has |MOJO_READ_DATA_FLAG_QUERY| set, it queries the amount of data
// available, setting |*num_bytes| to the number of bytes available. In this
// case, |MOJO_READ_DATA_FLAG_DISCARD| must not be set, and
// |MOJO_READ_DATA_FLAG_ALL_OR_NONE| is ignored, as are |elements| and the input
// value of |*num_bytes|.
//
// Returns:
//   |MOJO_RESULT_OK| on success (see above for a description of the different
//       operations).
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |data_pipe_consumer_handle| is invalid, the combination of flags in
//       |flags| is invalid, etc.).
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe producer handle has been
//       closed and data (or the required amount of data) was not available to
//       be read or discarded.
//   |MOJO_RESULT_OUT_OF_RANGE| if |flags| has |MOJO_READ_DATA_FLAG_ALL_OR_NONE|
//       set and the required amount of data is not available to be read or
//       discarded (and the producer is still open).
//   |MOJO_RESULT_BUSY| if there is a two-phase read ongoing with
//       |data_pipe_consumer_handle| (i.e., |MojoBeginReadData()| has been
//       called, but not yet the matching |MojoEndReadData()|).
//   |MOJO_RESULT_SHOULD_WAIT| if there is no data to be read or discarded (and
//       the producer is still open) and |flags| does *not* have
//       |MOJO_READ_DATA_FLAG_ALL_OR_NONE| set.
MOJO_SYSTEM_EXPORT MojoResult MojoReadData(MojoHandle data_pipe_consumer_handle,
                                           void* elements,       // Out.
                                           uint32_t* num_bytes,  // In/out.
                                           MojoReadDataFlags flags);

// Begins a two-phase read from the data pipe consumer given by
// |data_pipe_consumer_handle|. On success, |*buffer| will be a pointer from
// which the caller can read up to |*buffer_num_bytes| bytes of data.
//
// During a two-phase read, |data_pipe_consumer_handle| is *not* readable.
// If another caller tries to read from it by calling |MojoReadData()| or
// |MojoBeginReadData()|, their request will fail with |MOJO_RESULT_BUSY|.
//
// Once the caller has finished reading data from |*buffer|, |MojoEndReadData()|
// must be called to indicate the number of bytes read and to complete the
// two-phase read operation.
//
// |flags| must not have |MOJO_READ_DATA_FLAG_DISCARD|,
// |MOJO_READ_DATA_FLAG_QUERY|, |MOJO_READ_DATA_FLAG_PEEK|, or
// |MOJO_READ_DATA_FLAG_ALL_OR_NONE| set.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |data_pipe_consumer_handle| is not a handle to a data pipe consumer,
//       or |flags| has invalid flags set.)
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe producer handle has been
//       closed.
//   |MOJO_RESULT_BUSY| if there is already a two-phase read ongoing with
//       |data_pipe_consumer_handle| (i.e., |MojoBeginReadData()| has been
//       called, but not yet the matching |MojoEndReadData()|).
//   |MOJO_RESULT_SHOULD_WAIT| if no data can currently be read (and the
//       producer is still open).
MOJO_SYSTEM_EXPORT MojoResult
MojoBeginReadData(MojoHandle data_pipe_consumer_handle,
                  const void** buffer,         // Out.
                  uint32_t* buffer_num_bytes,  // Out.
                  MojoReadDataFlags flags);

// Ends a two-phase read from the data pipe consumer given by
// |data_pipe_consumer_handle| that was begun by a call to |MojoBeginReadData()|
// on the same handle. |num_bytes_read| should indicate the amount of data
// actually read; it must be less than or equal to the value of
// |*buffer_num_bytes| output by |MojoBeginReadData()| and must be a multiple of
// the element size.
//
// On failure, the two-phase read (if any) is ended (so the handle may become
// readable again) but no data is "removed" from the data pipe.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |data_pipe_consumer_handle| is not a handle to a data pipe consumer or
//       |num_bytes_written| is greater than the maximum value provided by
//       |MojoBeginReadData()| or not a multiple of the element size).
//   |MOJO_RESULT_FAILED_PRECONDITION| if the data pipe consumer is not in a
//       two-phase read (e.g., |MojoBeginReadData()| was not called or
//       |MojoEndReadData()| has already been called).
MOJO_SYSTEM_EXPORT MojoResult
    MojoEndReadData(MojoHandle data_pipe_consumer_handle,
                    uint32_t num_bytes_read);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_DATA_PIPE_H_
