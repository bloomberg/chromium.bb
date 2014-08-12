// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EMBEDDER_EMBEDDER_H_
#define MOJO_EMBEDDER_EMBEDDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "mojo/embedder/scoped_platform_handle.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace embedder {

// Must be called first to initialize the (global, singleton) system.
MOJO_SYSTEM_IMPL_EXPORT void Init();

// A "channel" is a connection on top of an OS "pipe", on top of which Mojo
// message pipes (etc.) can be multiplexed. It must "live" on some I/O thread.
//
// There are two "channel" creation/destruction APIs: the synchronous
// |CreateChannelOnIOThread()|/|DestroyChannelOnIOThread()|, which must be
// called from the I/O thread, and the asynchronous
// |CreateChannel()|/|DestroyChannel()|, which may be called from any thread.
//
// Both creation functions have a |platform_handle| argument, which should be an
// OS-dependent handle to one side of a suitable bidirectional OS "pipe" (e.g.,
// a file descriptor to a socket on POSIX, a handle to a named pipe on Windows);
// this "pipe" should be connected and ready for operation (e.g., to be written
// to or read from).
//
// Both (synchronously) return a handle to the bootstrap message pipe on the
// channel that was (or is to be) created, or |MOJO_HANDLE_INVALID| on error
// (but note that this will happen only if, e.g., the handle table is full).
// This message pipe may be used immediately, but since channel operation
// actually begins asynchronously, other errors may still occur (e.g., if the
// other end of the "pipe" is closed) and be reported in the usual way to the
// returned handle.
//
// (E.g., a message written immediately to the returned handle will be queued
// and the handle immediately closed, before the channel begins operation. In
// this case, the channel should connect as usual, send the queued message, and
// report that the handle was closed to the other side. The message sent may
// have other handles, so there may still be message pipes "on" this channel.)
//
// Both also produce a |ChannelInfo*| (a pointer to an opaque object) -- the
// first synchronously and second asynchronously.
//
// The destruction functions are similarly synchronous and asynchronous,
// respectively, and take the |ChannelInfo*| produced by the creation function.
// (Note: One may call |DestroyChannelOnIOThread()| with the result of
// |CreateChannel()|, but not |DestroyChannel()| with the result of
// |CreateChannelOnIOThread()|.)
//
// TODO(vtl): Figure out channel teardown.
struct ChannelInfo;

// Creates a channel; must only be called from the I/O thread. |platform_handle|
// should be a handle to a connected OS "pipe". Eventually (even on failure),
// the "out" value |*channel_info| should be passed to
// |DestroyChannelOnIOThread()| to tear down the channel. Returns a handle to
// the bootstrap message pipe.
MOJO_SYSTEM_IMPL_EXPORT ScopedMessagePipeHandle
    CreateChannelOnIOThread(ScopedPlatformHandle platform_handle,
                            ChannelInfo** channel_info);

typedef base::Callback<void(ChannelInfo*)> DidCreateChannelCallback;
// Creates a channel asynchronously; may be called from any thread.
// |platform_handle| should be a handle to a connected OS "pipe".
// |io_thread_task_runner| should be the |TaskRunner| for the I/O thread.
// |callback| should be the callback to call with the |ChannelInfo*|, which
// should eventually be passed to |DestroyChannel()| (or
// |DestroyChannelOnIOThread()|) to tear down the channel; the callback will be
// called using |callback_thread_task_runner| if that is non-null, or otherwise
// it will be called using |io_thread_task_runner|. Returns a handle to the
// bootstrap message pipe.
MOJO_SYSTEM_IMPL_EXPORT ScopedMessagePipeHandle
    CreateChannel(ScopedPlatformHandle platform_handle,
                  scoped_refptr<base::TaskRunner> io_thread_task_runner,
                  DidCreateChannelCallback callback,
                  scoped_refptr<base::TaskRunner> callback_thread_task_runner);

// Destroys a channel that was created using either |CreateChannelOnIOThread()|
// or |CreateChannel()|; must only be called from the I/O thread. |channel_info|
// should be the "out" value from |CreateChannelOnIOThread()| or the value
// provided to the callback to |CreateChannel()|.
MOJO_SYSTEM_IMPL_EXPORT void DestroyChannelOnIOThread(
    ChannelInfo* channel_info);

// Destroys a channel (asynchronously) that was created using |CreateChannel()|
// (note: NOT |CreateChannelOnIOThread()|); may be called from any thread.
// |channel_info| should be the value provided to the callback to
// |CreateChannel()|.
MOJO_SYSTEM_IMPL_EXPORT void DestroyChannel(ChannelInfo* channel_info);

// Creates a |MojoHandle| that wraps the given |PlatformHandle| (taking
// ownership of it). This |MojoHandle| can then, e.g., be passed through message
// pipes. Note: This takes ownership (and thus closes) |platform_handle| even on
// failure, which is different from what you'd expect from a Mojo API, but it
// makes for a more convenient embedder API.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
    CreatePlatformHandleWrapper(ScopedPlatformHandle platform_handle,
                                MojoHandle* platform_handle_wrapper_handle);
// Retrieves the |PlatformHandle| that was wrapped into a |MojoHandle| (using
// |CreatePlatformHandleWrapper()| above). Note that the |MojoHandle| must still
// be closed separately.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
    PassWrappedPlatformHandle(MojoHandle platform_handle_wrapper_handle,
                              ScopedPlatformHandle* platform_handle);

}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_EMBEDDER_EMBEDDER_H_
