// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_MESSAGE_PIPE_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_MESSAGE_PIPE_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/system/awakable_list.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/message_in_transit_queue.h"
#include "mojo/edk/system/raw_channel.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/macros.h"

namespace base {
namespace debug {
class StackTrace;
}
}

namespace mojo {
namespace edk {

// This is the |Dispatcher| implementation for message pipes (created by the
// Mojo primitive |MojoCreateMessagePipe()|). This class is thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT MessagePipeDispatcher final
    : public Dispatcher, public RawChannel::Delegate {
 public:
  // The default options to use for |MojoCreateMessagePipe()|. (Real uses
  // should obtain this via |ValidateCreateOptions()| with a null |in_options|;
  // this is exposed directly for testing convenience.)
  static const MojoCreateMessagePipeOptions kDefaultCreateOptions;

  static scoped_refptr<MessagePipeDispatcher> Create(
      const MojoCreateMessagePipeOptions& validated_options) {
    return make_scoped_refptr(new MessagePipeDispatcher(
        !!(validated_options.flags &
           MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_TRANSFERABLE)));
  }

  // Validates and/or sets default options for |MojoCreateMessagePipeOptions|.
  // If non-null, |in_options| must point to a struct of at least
  // |in_options->struct_size| bytes. |out_options| must point to a (current)
  // |MojoCreateMessagePipeOptions| and will be entirely overwritten on success
  // (it may be partly overwritten on failure).
  static MojoResult ValidateCreateOptions(
      const MojoCreateMessagePipeOptions* in_options,
      MojoCreateMessagePipeOptions* out_options);

  // Initializes a transferable message pipe.
  // Must be called before any other methods. (This method is not thread-safe.)
  void Init(
      ScopedPlatformHandle message_pipe,
      char* serialized_read_buffer, size_t serialized_read_buffer_size,
      char* serialized_write_buffer, size_t serialized_write_buffer_size,
      std::vector<int>* serialized_read_fds,
      std::vector<int>* serialized_write_fds);

  // Initializes a nontransferable message pipe.
  void InitNonTransferable(uint64_t pipe_id);

  // |Dispatcher| public methods:
  Type GetType() const override;

  // RawChannel::Delegate methods:
  void OnReadMessage(
      const MessageInTransit::View& message_view,
      ScopedPlatformHandleVectorPtr platform_handles) override;
  void OnError(Error error) override;

  // Called by broker when a route is established between this
  // MessagePipeDispatcher and another one. This object will receive messages
  // sent to its pipe_id. It should tag all outgoing messages by calling
  // MessageInTransit::set_route_id with pipe_id_.
  void GotNonTransferableChannel(RawChannel* channel);

  // The "opposite" of |SerializeAndClose()|. (Typically this is called by
  // |Dispatcher::Deserialize()|.)
  static scoped_refptr<MessagePipeDispatcher> Deserialize(
      const void* source,
      size_t size,
      PlatformHandleVector* platform_handles);

 private:
  // See MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_TRANSFERABLE's definition for an
  // explanation of what is a transferable pipe.
  explicit MessagePipeDispatcher(bool transferable);
  ~MessagePipeDispatcher() override;

  void InitOnIO();
  void CloseOnIOAndRelease();
  void CloseOnIO();

  // |Dispatcher| protected methods:
  void CancelAllAwakablesNoLock() override;
  void CloseImplNoLock() override;
  scoped_refptr<Dispatcher> CreateEquivalentDispatcherAndCloseImplNoLock()
      override;
  MojoResult WriteMessageImplNoLock(
      const void* bytes,
      uint32_t num_bytes,
      std::vector<DispatcherTransport>* transports,
      MojoWriteMessageFlags flags) override;
  MojoResult ReadMessageImplNoLock(void* bytes,
                                   uint32_t* num_bytes,
                                   DispatcherVector* dispatchers,
                                   uint32_t* num_dispatchers,
                                   MojoReadMessageFlags flags) override;
  HandleSignalsState GetHandleSignalsStateImplNoLock() const override;
  MojoResult AddAwakableImplNoLock(Awakable* awakable,
                                   MojoHandleSignals signals,
                                   uintptr_t context,
                                   HandleSignalsState* signals_state) override;
  void RemoveAwakableImplNoLock(Awakable* awakable,
                                HandleSignalsState* signals_state) override;
  void StartSerializeImplNoLock(size_t* max_size,
                                size_t* max_platform_handles) override;
  bool EndSerializeAndCloseImplNoLock(
      void* destination,
      size_t* actual_size,
      PlatformHandleVector* platform_handles) override;
  void TransportStarted() override;
  void TransportEnded() override;

  // Calls ReleaseHandle and serializes the raw channel. This is split into a
  // function because it's called in two different ways:
  // 1) When serializing "live" dispatchers that are passed to MojoWriteMessage,
  // CreateEquivalentDispatcherAndCloseImplNoLock calls this.
  // 2) When serializing dispatchers that are attached to deserialized messages
  // which haven't been consumed by MojoReadMessage, StartSerializeImplNoLock
  // calls this.
  void SerializeInternal();

  MojoResult AttachTransportsNoLock(
      MessageInTransit* message,
      std::vector<DispatcherTransport>* transports);

  // Called whenever a read or write is done on a non-transferable pipe, which
  // "binds" the pipe id to this object.
  void RequestNontransferableChannel();

  // Protected by |lock()|:
  RawChannel* channel_;

  // Queue of incoming messages that we read from RawChannel but haven't been
  // consumed through MojoReadMessage yet.
  MessageInTransitQueue message_queue_;

  // The following members are only used when transferable_ is true;

  // When sending MP, contains serialized message_queue_.
  std::vector<char> serialized_message_queue_;
  std::vector<char> serialized_read_buffer_;
  std::vector<char> serialized_write_buffer_;
  // Contains FDs from (in this order): the read buffer, the write buffer, and
  // message queue.
  std::vector<int> serialized_fds_;
  size_t serialized_read_fds_length_;
  size_t serialized_write_fds_length_;
  size_t serialized_message_fds_length_;
  ScopedPlatformHandle serialized_platform_handle_;

  // The following members are only used when transferable_ is false;

  // The unique id shared by both ends of a non-transferable message pipe. This
  // is held on until a read or write are done, and at that point it's used to
  // get a RawChannel.
  uint64_t pipe_id_;
  enum NonTransferableState {
    // The pipe_id hasn't been bound to this object yet until it's read,
    // written, or waited on.
    WAITING_FOR_READ_OR_WRITE,
    // This object was interacted with, so the pipe_id has been bound and we are
    // waiting for the broker to connect both sides.
    CONNECT_CALLED,
    // We have a connection to the other end of the message pipe.
    CONNECTED,
    // This object has been closed before it's connected. To ensure that the
    // other end receives a closed message from this end, we've initiated
    // connecting and will close after it succeeds.
    WAITING_FOR_CONNECT_TO_CLOSE,
    // The message pipe is closed.
    CLOSED,
    // The message pipe has been transferred.
    SERIALISED,
  };

  NonTransferableState non_transferable_state_;
  // Messages that were written while we were waiting to get a RawChannel.
  MessageInTransitQueue non_transferable_outgoing_message_queue_;
  scoped_ptr<base::debug::StackTrace> non_transferable_bound_stack_;


  // The following members are used for both modes of transferable_.

  AwakableList awakable_list_;

  // If DispatcherTransport is created. Must be set before lock() is called to
  // avoid deadlocks with RawChannel calling us.
  base::Lock started_transport_;

  bool serialized_;
  bool calling_init_;
  bool write_error_;
  // Whether it can be sent after read or write.
  bool transferable_;
  // When this object is closed, it has to wait to flush any pending messages
  // from the other side to ensure that any in-queue message pipes are closed.
  // If this is true, we have already sent the other side the request.
  bool close_requested_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(MessagePipeDispatcher);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_MESSAGE_PIPE_DISPATCHER_H_
