// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_RAW_CHANNEL_H_
#define MOJO_SYSTEM_RAW_CHANNEL_H_

#include <vector>

#include "base/basictypes.h"
#include "mojo/system/constants.h"
#include "mojo/system/scoped_platform_handle.h"
#include "mojo/system/system_impl_export.h"

namespace base {
class MessageLoop;
}

namespace mojo {
namespace system {

class MessageInTransit;

// This simply wraps an |int| file descriptor on POSIX and a |HANDLE| on
// Windows, but we don't want to impose, e.g., the inclusion of windows.h on
// everyone.
struct PlatformHandle;

// |RawChannel| is an interface to objects that wrap an OS "pipe". It presents
// the following interface to users:
//  - Receives and dispatches messages on a thread (running a |MessageLoop|; it
//    must be a |MessageLoopForIO| in the case of the POSIX libevent
//    implementation).
//  - Provides a thread-safe way of writing messages (|WriteMessage()|);
//    writing/queueing messages will not block and is atomic from the point of
//    view of the caller. If necessary, messages are queued (to be written on
//    the aforementioned thread).
//
// OS-specific implementation subclasses are to be instantiated using the
// |Create()| static factory method.
//
// With the exception of |WriteMessage()|, this class is thread-unsafe (and in
// general its methods should only be used on the I/O thread).
class MOJO_SYSTEM_IMPL_EXPORT RawChannel {
 public:
  virtual ~RawChannel() {}

  // The |Delegate| is only accessed on the same thread as the message loop
  // (passed in on creation).
  class MOJO_SYSTEM_IMPL_EXPORT Delegate {
   public:
    enum FatalError {
      FATAL_ERROR_UNKNOWN = 0,
      FATAL_ERROR_FAILED_READ,
      FATAL_ERROR_FAILED_WRITE
    };

    // Called when a message is read. This may call |Shutdown()| on the
    // |RawChannel|, but must not destroy it.
    virtual void OnReadMessage(const MessageInTransit& message) = 0;

    // Called when there's a fatal error, which leads to the channel no longer
    // being viable.
    virtual void OnFatalError(FatalError fatal_error) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Static factory method. |handle| should be a handle to a
  // (platform-appropriate) bidirectional communication channel (e.g., a socket
  // on POSIX, a named pipe on Windows). Does *not* take ownership of |delegate|
  // and |message_loop|, which must remain alive while this object does.
  static RawChannel* Create(ScopedPlatformHandle handle,
                            Delegate* delegate,
                            base::MessageLoop* message_loop);

  // This must be called (on the I/O thread) before this object is used. Returns
  // true on success. On failure, |Shutdown()| should *not* be called.
  virtual bool Init() = 0;

  // This must be called (on the I/O thread) before this object is destroyed.
  virtual void Shutdown() = 0;

  // This is thread-safe. It takes ownership of |message| (always, even on
  // failure). Returns true on success.
  virtual bool WriteMessage(MessageInTransit* message) = 0;

 protected:
  RawChannel(Delegate* delegate, base::MessageLoop* message_loop)
      : delegate_(delegate), message_loop_(message_loop) {}

  Delegate* delegate() { return delegate_; }
  base::MessageLoop* message_loop() { return message_loop_; }

 private:
  Delegate* const delegate_;
  base::MessageLoop* const message_loop_;

  DISALLOW_COPY_AND_ASSIGN(RawChannel);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_RAW_CHANNEL_H_
