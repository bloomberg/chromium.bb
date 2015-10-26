// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_MESSAGE_PIPE_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_MESSAGE_PIPE_DISPATCHER_H_

#include "base/memory/ref_counted.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/system/awakable_list.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/raw_channel.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/macros.h"

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
      const MojoCreateMessagePipeOptions& /*validated_options*/) {
    return make_scoped_refptr(new MessagePipeDispatcher());
  }

  // Validates and/or sets default options for |MojoCreateMessagePipeOptions|.
  // If non-null, |in_options| must point to a struct of at least
  // |in_options->struct_size| bytes. |out_options| must point to a (current)
  // |MojoCreateMessagePipeOptions| and will be entirely overwritten on success
  // (it may be partly overwritten on failure).
  static MojoResult ValidateCreateOptions(
      const MojoCreateMessagePipeOptions* in_options,
      MojoCreateMessagePipeOptions* out_options);

  // Must be called before any other methods. (This method is not thread-safe.)
  void Init(
      ScopedPlatformHandle message_pipe,
      char* serialized_read_buffer, size_t serialized_read_buffer_size,
      char* serialized_write_buffer, size_t serialized_write_buffer_size,
      std::vector<int>* serialized_read_fds,
      std::vector<int>* serialized_write_fds);

  // |Dispatcher| public methods:
  Type GetType() const override;

  // The "opposite" of |SerializeAndClose()|. (Typically this is called by
  // |Dispatcher::Deserialize()|.)
  static scoped_refptr<MessagePipeDispatcher> Deserialize(
      const void* source,
      size_t size,
      PlatformHandleVector* platform_handles);

 private:
  MessagePipeDispatcher();
  ~MessagePipeDispatcher() override;

  void InitOnIO();
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
                                   uint32_t context,
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

  // |RawChannel::Delegate methods:
  void OnReadMessage(
    const MessageInTransit::View& message_view,
    ScopedPlatformHandleVectorPtr platform_handles) override;
  void OnError(Error error) override;

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

  // Protected by |lock()|:
  RawChannel* channel_;

  // Queue of incoming messages that we read from RawChannel but haven't been
  // consumed through MojoReadMessage yet.
  MessageInTransitQueue message_queue_;
  // When sending MP, contains serialized message_queue_.
  bool serialized_;
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
  AwakableList awakable_list_;

  // If DispatcherTransport is created. Must be set before lock() is called to
  // avoid deadlocks with RawChannel calling us.
  base::Lock started_transport_;

  bool calling_init_;
  bool write_error_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(MessagePipeDispatcher);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_MESSAGE_PIPE_DISPATCHER_H_
