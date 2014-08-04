// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_PIPE_READER_H_
#define IPC_IPC_MESSAGE_PIPE_READER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "mojo/public/c/environment/async_waiter.h"
#include "mojo/public/cpp/system/core.h"

namespace IPC {
namespace internal {

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
  // Delay the object deletion using the current message loop.
  // This is intended to used by MessagePipeReader owners.
  class DelayedDeleter {
   public:
    typedef base::DefaultDeleter<MessagePipeReader> DefaultType;

    static void DeleteNow(MessagePipeReader* ptr) { delete ptr; }

    DelayedDeleter() {}
    DelayedDeleter(const DefaultType&) {}
    DelayedDeleter& operator=(const DefaultType&) { return *this; }

    void operator()(MessagePipeReader* ptr) const;
  };

  explicit MessagePipeReader(mojo::ScopedMessagePipeHandle handle);
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

  //
  // The client have to implment these callback to get the readiness
  // event from the reader
  //
  virtual void OnMessageReceived() = 0;
  virtual void OnPipeClosed() = 0;
  virtual void OnPipeError(MojoResult error) = 0;

 private:
  static void InvokePipeIsReady(void* closure, MojoResult result);

  MojoResult ReadMessageBytes();
  void PipeIsReady(MojoResult wait_result);
  void StartWaiting();
  void StopWaiting();

  std::vector<char>  data_buffer_;
  std::vector<MojoHandle> handle_buffer_;
  MojoAsyncWaitID pipe_wait_id_;
  mojo::ScopedMessagePipeHandle pipe_;

  DISALLOW_COPY_AND_ASSIGN(MessagePipeReader);
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_PIPE_READER_H_
