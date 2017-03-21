// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_EMBEDDER_EMBEDDER_H_
#define MOJO_EDK_EMBEDDER_EMBEDDER_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_handle.h"
#include "base/process/process_handle.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/pending_process_connection.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace base {
class PortProvider;
}

namespace mojo {
namespace edk {

// Basic configuration/initialization ------------------------------------------

// |Init()| sets up the basic Mojo system environment, making the |Mojo...()|
// functions available and functional. This is never shut down (except in tests
// -- see test_embedder.h).

// Allows changing the default max message size. Must be called before Init.
MOJO_SYSTEM_IMPL_EXPORT void SetMaxMessageSize(size_t bytes);

// Should be called as early as possible in a child process with a handle to the
// other end of a pipe provided in the parent to
// PendingProcessConnection::Connect.
MOJO_SYSTEM_IMPL_EXPORT void SetParentPipeHandle(ScopedPlatformHandle pipe);

// Same as above but extracts the pipe handle from the command line. See
// PlatformChannelPair for details.
MOJO_SYSTEM_IMPL_EXPORT void SetParentPipeHandleFromCommandLine();

// Called to connect to a peer process. This should be called only if there
// is no common ancestor for the processes involved within this mojo system.
// Both processes must call this function, each passing one end of a platform
// channel. This returns one end of a message pipe to each process.
MOJO_SYSTEM_IMPL_EXPORT ScopedMessagePipeHandle
ConnectToPeerProcess(ScopedPlatformHandle pipe);

// Called to connect to a peer process. This should be called only if there
// is no common ancestor for the processes involved within this mojo system.
// Both processes must call this function, each passing one end of a platform
// channel. This returns one end of a message pipe to each process. |peer_token|
// may be passed to ClosePeerConnection() to close the connection.
MOJO_SYSTEM_IMPL_EXPORT ScopedMessagePipeHandle
ConnectToPeerProcess(ScopedPlatformHandle pipe, const std::string& peer_token);

// Closes a connection to a peer process created by ConnectToPeerProcess()
// where the same |peer_token| was used.
MOJO_SYSTEM_IMPL_EXPORT void ClosePeerConnection(const std::string& peer_token);

// Must be called first, or just after setting configuration parameters, to
// initialize the (global, singleton) system.
MOJO_SYSTEM_IMPL_EXPORT void Init();

// Sets a default callback to invoke when an internal error is reported but
// cannot be associated with a specific child process.
MOJO_SYSTEM_IMPL_EXPORT void SetDefaultProcessErrorCallback(
    const ProcessErrorCallback& callback);

// Basic functions -------------------------------------------------------------

// The functions in this section are available once |Init()| has been called.

// Creates a |MojoHandle| that wraps the given |PlatformHandle| (taking
// ownership of it). This |MojoHandle| can then, e.g., be passed through message
// pipes. Note: This takes ownership (and thus closes) |platform_handle| even on
// failure, which is different from what you'd expect from a Mojo API, but it
// makes for a more convenient embedder API.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
CreatePlatformHandleWrapper(ScopedPlatformHandle platform_handle,
                            MojoHandle* platform_handle_wrapper_handle);

// Retrieves the |PlatformHandle| that was wrapped into a |MojoHandle| (using
// |CreatePlatformHandleWrapper()| above). Note that the |MojoHandle| is closed
// on success.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
PassWrappedPlatformHandle(MojoHandle platform_handle_wrapper_handle,
                          ScopedPlatformHandle* platform_handle);

// Creates a |MojoHandle| that wraps the given |SharedMemoryHandle| (taking
// ownership of it). |num_bytes| is the size of the shared memory object, and
// |read_only| is whether the handle is a read-only handle to shared memory.
// This |MojoHandle| is a Mojo shared buffer and can be manipulated using the
// shared buffer functions and transferred over a message pipe.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
CreateSharedBufferWrapper(base::SharedMemoryHandle shared_memory_handle,
                          size_t num_bytes,
                          bool read_only,
                          MojoHandle* mojo_wrapper_handle);

// Retrieves the underlying |SharedMemoryHandle| from a shared buffer
// |MojoHandle| and closes the handle. If successful, |num_bytes| will contain
// the size of the shared memory buffer and |read_only| will contain whether the
// buffer handle is read-only. Both |num_bytes| and |read_only| may be null.
// Note: The value of |shared_memory_handle| may be
// base::SharedMemory::NULLHandle(), even if this function returns success.
// Callers should perform appropriate checks.
MOJO_SYSTEM_IMPL_EXPORT MojoResult
PassSharedMemoryHandle(MojoHandle mojo_handle,
                       base::SharedMemoryHandle* shared_memory_handle,
                       size_t* num_bytes,
                       bool* read_only);

// Initialialization/shutdown for interprocess communication (IPC) -------------

// |InitIPCSupport()| sets up the subsystem for interprocess communication,
// making the IPC functions (in the following section) available and functional.
// (This may only be done after |Init()|.)
//
// This subsystem may be shut down using |ShutdownIPCSupport()|. None of the IPC
// functions may be called after this is called.
//
// |io_thread_task_runner| should live at least until |ShutdownIPCSupport()|'s
// callback has been run.
MOJO_SYSTEM_IMPL_EXPORT void InitIPCSupport(
    scoped_refptr<base::TaskRunner> io_thread_task_runner);

// Retrieves the TaskRunner used for IPC I/O, as set by InitIPCSupport.
MOJO_SYSTEM_IMPL_EXPORT scoped_refptr<base::TaskRunner> GetIOTaskRunner();

// Shuts down the subsystem initialized by |InitIPCSupport()|. It be called from
// any thread and will attempt to complete shutdown on the I/O thread with which
// the system was initialized. Upon completion, |callback| is invoked on an
// arbitrary thread.
MOJO_SYSTEM_IMPL_EXPORT void ShutdownIPCSupport(const base::Closure& callback);

#if defined(OS_MACOSX) && !defined(OS_IOS)
// Set the |base::PortProvider| for this process. Can be called on any thread,
// but must be set in the root process before any Mach ports can be transferred.
MOJO_SYSTEM_IMPL_EXPORT void SetMachPortProvider(
    base::PortProvider* port_provider);
#endif

// Creates a message pipe from a token in a child process. This token must have
// been acquired by a corresponding call to
// PendingProcessConnection::CreateMessagePipe.
MOJO_SYSTEM_IMPL_EXPORT ScopedMessagePipeHandle
CreateChildMessagePipe(const std::string& token);

// Generates a random ASCII token string for use with various APIs that expect
// a globally unique token string.
MOJO_SYSTEM_IMPL_EXPORT std::string GenerateRandomToken();

// Sets system properties that can be read by the MojoGetProperty() API. See the
// documentation for MojoPropertyType for supported property types and their
// corresponding value type.
//
// Default property values:
//   |MOJO_PROPERTY_TYPE_SYNC_CALL_ALLOWED| - true
MOJO_SYSTEM_IMPL_EXPORT MojoResult SetProperty(MojoPropertyType type,
                                               const void* value);

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_EMBEDDER_EMBEDDER_H_
