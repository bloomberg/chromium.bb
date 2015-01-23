// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_PIPE_READER_H_
#define IPC_IPC_MESSAGE_PIPE_READER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_message.h"
#include "third_party/mojo/src/mojo/public/c/environment/async_waiter.h"
#include "third_party/mojo/src/mojo/public/cpp/system/core.h"

namespace IPC {
namespace internal {

class AsyncHandleWaiter;

// A helper class to handle bytestream directly over mojo::MessagePipe
// in template-method pattern. MessagePipeReader manages the lifetime
// of given MessagePipe and participates the event loop, and
// read the stream and call the client when it is ready.
//
// Each client has to:
//
//  * Provide a subclass implemenation of a specific use of a MessagePipe
//    and implement callbacks.
//  * Create the subclass instance with a MessagePipeHandle.
//    The constructor automatically start listening on the pipe.
//
// MessageReader has to be used in IO thread. It isn't thread-safe.
//
class MessagePipeReader {
 public:
  class Delegate {
   public:
    virtual void OnMessageReceived(Message& message) = 0;
    virtual void OnPipeClosed(MessagePipeReader* reader) = 0;
    virtual void OnPipeError(MessagePipeReader* reader) = 0;
  };

  // Delay the object deletion using the current message loop.
  // This is intended to used by MessagePipeReader owners.
  class DelayedDeleter {
   public:
    typedef base::DefaultDeleter<MessagePipeReader> DefaultType;

    static void DeleteNow(MessagePipeReader* ptr) { delete ptr; }

    DelayedDeleter() {}
    explicit DelayedDeleter(const DefaultType&) {}
    DelayedDeleter& operator=(const DefaultType&) { return *this; }

    void operator()(MessagePipeReader* ptr) const;
  };

  // Both parameters must be non-null.
  // Build a reader that reads messages from |handle| and lets |delegate| know.
  // Note that MessagePipeReader doesn't delete |delete|.
  MessagePipeReader(mojo::ScopedMessagePipeHandle handle, Delegate* delegate);
  virtual ~MessagePipeReader();

  MojoHandle handle() const { return pipe_.get().value(); }

  // Returns received bytes.
  const std::vector<char>& data_buffer() const {
    return data_buffer_;
  }

  // Delegate received handles ownership. The subclass should take the
  // ownership over in its OnMessageReceived(). They will leak otherwise.
  void TakeHandleBuffer(std::vector<MojoHandle>* handle_buffer) {
    handle_buffer_.swap(*handle_buffer);
  }

  // Close and destroy the MessagePipe.
  void Close();
  // Close the mesage pipe with notifying the client with the error.
  void CloseWithError(MojoResult error);
  // Return true if the MessagePipe is alive.
  bool IsValid() { return pipe_.is_valid(); }

  bool Send(scoped_ptr<Message> message);
  void ReadMessagesThenWait();

 private:
  void OnMessageReceived();
  void OnPipeClosed();
  void OnPipeError(MojoResult error);

  MojoResult ReadMessageBytes();
  void PipeIsReady(MojoResult wait_result);
  void ReadAvailableMessages();

  std::vector<char>  data_buffer_;
  std::vector<MojoHandle> handle_buffer_;
  mojo::ScopedMessagePipeHandle pipe_;
  // |delegate_| and |async_waiter_| are null once the message pipe is closed.
  Delegate* delegate_;
  scoped_ptr<AsyncHandleWaiter> async_waiter_;

  DISALLOW_COPY_AND_ASSIGN(MessagePipeReader);
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_PIPE_READER_H_
