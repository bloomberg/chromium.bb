// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_TRANSPORT_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_TRANSPORT_H_

#include <queue>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "extensions/browser/api/cast_channel/logger.h"
#include "extensions/common/api/cast_channel.h"
#include "net/base/completion_callback.h"

namespace net {
class DrainableIOBuffer;
class IPEndPoint;
class IOBuffer;
class DrainableIOBuffer;
class GrowableIOBuffer;
}  // namespace net

namespace extensions {
namespace core_api {
namespace cast_channel {
class CastMessage;
struct LastErrors;
class Logger;
class MessageFramer;

// TODO(kmarshall): Migrate CastSocket to new interface.
// Redirect references to CastSocket in logger.h to this interface once
// the interface is promoted to cast_socket.h.
class CastSocketInterface {
 public:
  CastSocketInterface() {}
  virtual ~CastSocketInterface() {}

  // Writes at least one, and up to |size| bytes to the socket.
  // Returns net::ERR_IO_PENDING if the operation will complete
  //     asynchronously, in which case |callback| will be invoked
  //     on completion.
  // Asynchronous writes are cancleled if the CastSocket is deleted.
  // All values <= zero indicate an error.
  virtual int Write(net::IOBuffer* buffer,
                    size_t size,
                    const net::CompletionCallback& callback) = 0;

  // Reads at least one, and up to |size| bytes from the socket.
  // Returns net::ERR_IO_PENDING if the operation will complete
  // asynchronously, in which case |callback| will be invoked
  // on completion.
  // All values <= zero indicate an error.
  virtual int Read(net::IOBuffer* buf,
                   int buf_len,
                   const net::CompletionCallback& callback) = 0;
  virtual void CloseWithError(ChannelError error) = 0;
  virtual const net::IPEndPoint& ip_endpoint() const = 0;
  virtual ChannelAuthType channel_auth() const = 0;
  virtual int id() const = 0;
};

// Manager class for reading and writing messages to/from a CastSocket.
class CastTransport {
 public:
  // Object to be informed of incoming messages and errors.
  class Delegate {
   public:
    // An error occurred on the channel. |last_errors| contains the last errors
    // logged for the channel from the implementation.
    virtual void OnError(const CastSocketInterface* socket,
                         ChannelError error_state,
                         const LastErrors& last_errors) = 0;
    // A message was received on the channel.
    virtual void OnMessage(const CastSocketInterface* socket,
                           const CastMessage& message) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Adds a CastMessage read/write layer to a socket.
  // Message read events are propagated to the owner via |read_delegate|.
  // The CastTransport object should be deleted prior to the
  // underlying socket being deleted.
  CastTransport(CastSocketInterface* socket,
                Delegate* read_delegate,
                scoped_refptr<Logger> logger);
  virtual ~CastTransport();

  // Sends a CastMessage to |socket_|.
  // |message|: The message to send.
  // |callback|: Callback to be invoked when the write operation has finished.
  void SendMessage(const CastMessage& message,
                   const net::CompletionCallback& callback);

  // Starts reading messages from |socket_|.
  void StartReadLoop();

 private:
  // Internal write states.
  enum WriteState {
    WRITE_STATE_NONE,
    WRITE_STATE_WRITE,
    WRITE_STATE_WRITE_COMPLETE,
    WRITE_STATE_DO_CALLBACK,
    WRITE_STATE_ERROR,
  };

  // Internal read states.
  enum ReadState {
    READ_STATE_NONE,
    READ_STATE_READ,
    READ_STATE_READ_COMPLETE,
    READ_STATE_DO_CALLBACK,
    READ_STATE_ERROR,
  };

  // Holds a message to be written to the socket. |callback| is invoked when the
  // message is fully written or an error occurrs.
  struct WriteRequest {
    explicit WriteRequest(const std::string& namespace_,
                          const std::string& payload,
                          const net::CompletionCallback& callback);
    ~WriteRequest();

    // Namespace of the serialized message.
    std::string message_namespace;
    // Write completion callback, invoked when the operation has completed or
    // failed.
    net::CompletionCallback callback;
    // Buffer with outgoing data.
    scoped_refptr<net::DrainableIOBuffer> io_buffer;
  };

  static proto::ReadState ReadStateToProto(CastTransport::ReadState state);
  static proto::WriteState WriteStateToProto(CastTransport::WriteState state);
  static proto::ErrorState ErrorStateToProto(ChannelError state);

  // Terminates all in-flight write callbacks with error code ERR_FAILED.
  void FlushWriteQueue();

  // Main method that performs write flow state transitions.
  void OnWriteResult(int result);

  // Each of the below Do* method is executed in the corresponding
  // write state. For example when write state is WRITE_STATE_WRITE_COMPLETE
  // DowriteComplete is called, and so on.
  int DoWrite();
  int DoWriteComplete(int result);
  int DoWriteCallback();
  int DoWriteError(int result);

  // Main method that performs write flow state transitions.
  void OnReadResult(int result);

  // Each of the below Do* method is executed in the corresponding
  // write state. For example when write state is READ_STATE_READ_COMPLETE
  // DoReadComplete is called, and so on.
  int DoRead();
  int DoReadComplete(int result);
  int DoReadCallback();
  int DoReadError(int result);

  void SetReadState(ReadState read_state);
  void SetWriteState(WriteState write_state);
  void SetErrorState(ChannelError error_state);

  // Queue of pending writes. The message at the front of the queue is the one
  // being written.
  std::queue<WriteRequest> write_queue_;

  // Buffer used for read operations. Reused for every read.
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;

  // Constructs and parses the wire representation of message frames.
  scoped_ptr<MessageFramer> framer_;

  // Last message received on the socket.
  scoped_ptr<CastMessage> current_message_;

  // Socket used for I/O operations.
  CastSocketInterface* const socket_;

  // Methods for communicating message receipt and error status to client code.
  Delegate* const read_delegate_;

  // Write flow state machine state.
  WriteState write_state_;

  // Read flow state machine state.
  ReadState read_state_;

  // Most recent error that occurred during read or write operation, if any.
  ChannelError error_state_;

  scoped_refptr<Logger> logger_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(CastTransport);
};
}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_TRANSPORT_H_
