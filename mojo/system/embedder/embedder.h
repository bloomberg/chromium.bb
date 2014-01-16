// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_EMBEDDER_EMBEDDER_H_
#define MOJO_SYSTEM_EMBEDDER_EMBEDDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "mojo/public/system/core.h"
#include "mojo/system/embedder/scoped_platform_handle.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace embedder {

// Must be called first to initialize the (global, singleton) system.
MOJO_SYSTEM_IMPL_EXPORT void Init();

// Creates a new "channel", returning a handle to the bootstrap message pipe on
// that channel. |platform_handle| should be an OS-dependent handle to one side
// of a suitable bidirectional OS "pipe" (e.g., a file descriptor to a socket on
// POSIX, a handle to a named pipe on Windows); this "pipe" should be connected
// and ready for operation (e.g., to be written to or read from).
// |io_thread_task_runner| should be a |TaskRunner| for the thread on which the
// "channel" will run (read data and demultiplex).
//
// Returns |MOJO_HANDLE_INVALID| on error. Note that this will happen only if,
// e.g., the handle table is full (operation of the channel begins
// asynchronously and if, e.g., the other end of the "pipe" is closed, this will
// report an error to the returned handle in the usual way).
//
// Notes: The handle returned is ready for use immediately, with messages
// written to it queued. E.g., it would be perfectly valid for a message to be
// immediately written to the returned handle and the handle closed, all before
// the channel has begun operation on the IO thread. In this case, the channel
// is expected to connect as usual, send the queued message, and report that the
// handle was closed to the other side. (This message may well contain another
// handle, so there may well still be message pipes "on" this channel.)
//
// TODO(vtl): Figure out channel teardown.
struct ChannelInfo;
typedef base::Callback<void(ChannelInfo*)> DidCreateChannelOnIOThreadCallback;
MOJO_SYSTEM_IMPL_EXPORT MojoHandle CreateChannel(
    ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_thread_task_runner,
    DidCreateChannelOnIOThreadCallback callback);

MOJO_SYSTEM_IMPL_EXPORT void DestroyChannelOnIOThread(
    ChannelInfo* channel_info);

}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_SYSTEM_EMBEDDER_EMBEDDER_H_
