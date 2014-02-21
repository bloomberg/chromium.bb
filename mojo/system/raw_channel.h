// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_RAW_CHANNEL_H_
#define MOJO_SYSTEM_RAW_CHANNEL_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/system/constants.h"
#include "mojo/system/embedder/scoped_platform_handle.h"
#include "mojo/system/system_impl_export.h"

namespace base {
class MessageLoopForIO;
}

namespace mojo {
namespace system {

class MessageInTransit;

// |RawChannel| is an interface to objects that wrap an OS "pipe". It presents
// the following interface to users:
//  - Receives and dispatches messages on an I/O thread (running a
//    |MessageLoopForIO|.
//  - Provides a thread-safe way of writing messages (|WriteMessage()|);
//    writing/queueing messages will not block and is atomic from the point of
//    view of the caller. If necessary, messages are queued (to be written on
//    the aforementioned thread).
//
// OS-specific implementation subclasses are to be instantiated using the
// |Create()| static factory method.
//
// With the exception of |WriteMessage()|, this class is thread-unsafe (and in
// general its methods should only be used on the I/O thread, i.e., the thread
// on which |Init()| is called).
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
    // For each raw channel, at most one |FATAL_ERROR_FAILED_READ| and one
    // |FATAL_ERROR_FAILED_WRITE| notification will be issued. (And it is
    // possible to get both.)
    // After |OnFatalError(FATAL_ERROR_FAILED_READ)| there won't be further
    // |OnReadMessage()| calls.
    virtual void OnFatalError(FatalError fatal_error) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Static factory method. |handle| should be a handle to a
  // (platform-appropriate) bidirectional communication channel (e.g., a socket
  // on POSIX, a named pipe on Windows). Does *not* take ownership of |delegate|
  // and |message_loop_for_io|, which must remain alive while this object does.
  static RawChannel* Create(embedder::ScopedPlatformHandle handle,
                            Delegate* delegate,
                            base::MessageLoopForIO* message_loop_for_io);

  // This must be called (on an I/O thread) before this object is used. Returns
  // true on success. On failure, |Shutdown()| should *not* be called.
  virtual bool Init() = 0;

  // This must be called (on the I/O thread) before this object is destroyed.
  virtual void Shutdown() = 0;

  // This is thread-safe. It takes ownership of |message| (always, even on
  // failure). Returns true on success.
  virtual bool WriteMessage(scoped_ptr<MessageInTransit> message) = 0;

 protected:
  RawChannel(Delegate* delegate, base::MessageLoopForIO* message_loop_for_io)
      : delegate_(delegate), message_loop_for_io_(message_loop_for_io) {}

  Delegate* delegate() { return delegate_; }
  base::MessageLoopForIO* message_loop_for_io() { return message_loop_for_io_; }

 private:
  Delegate* const delegate_;
  base::MessageLoopForIO* const message_loop_for_io_;

  DISALLOW_COPY_AND_ASSIGN(RawChannel);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_RAW_CHANNEL_H_
