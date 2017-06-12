// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_USER_MESSAGE_IMPL_H_
#define MOJO_EDK_SYSTEM_USER_MESSAGE_IMPL_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/ports/event.h"
#include "mojo/edk/system/ports/name.h"
#include "mojo/edk/system/ports/port_ref.h"
#include "mojo/edk/system/ports/user_message.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace edk {

class NodeController;

// UserMessageImpl is the sole implementation of ports::UserMessage used to
// attach message data to any ports::UserMessageEvent.
//
// A UserMessageImpl may be either serialized or unserialized. Unserialized
// instances are serialized lazily only when necessary, i.e., if and when
// Serialize() is called to obtain a serialized message for wire transfer.
class MOJO_SYSTEM_IMPL_EXPORT UserMessageImpl
    : public NON_EXPORTED_BASE(ports::UserMessage) {
 public:
  static const TypeInfo kUserMessageTypeInfo;

  ~UserMessageImpl() override;

  // Creates a new ports::UserMessageEvent with an attached unserialized
  // UserMessageImpl associated with |context| and |thunks|.
  static std::unique_ptr<ports::UserMessageEvent>
  CreateEventForNewMessageWithContext(uintptr_t context,
                                      const MojoMessageOperationThunks* thunks);

  // Creates a new ports::UserMessageEvent with an attached serialized
  // UserMessageImpl. May fail iff one or more |dispatchers| fails to serialize
  // (e.g. due to it being in an invalid state.)
  //
  // Upon success, MOJO_RESULT_OK is returned and the new UserMessageEvent is
  // stored in |*out_event|.
  static MojoResult CreateEventForNewSerializedMessage(
      uint32_t num_bytes,
      const Dispatcher::DispatcherInTransit* dispatchers,
      uint32_t num_dispatchers,
      std::unique_ptr<ports::UserMessageEvent>* out_event);

  // Creates a new UserMessageImpl from an existing serialized message buffer
  // which was read from a Channel. Takes ownership of |channel_message|.
  // |payload| and |payload_size| represent the range of bytes within
  // |channel_message| which should be parsed by this call.
  static std::unique_ptr<UserMessageImpl> CreateFromChannelMessage(
      Channel::MessagePtr channel_message,
      void* payload,
      size_t payload_size);

  // Reads a message from |port| on |node_controller|'s Node.
  //
  // The message may or may not require deserialization. If the read message is
  // unserialized, it must have been sent from within the same process that's
  // receiving it and this call merely passes ownership of the message object
  // back out of the ports layer. In this case, |read_any_size| must be true,
  // |*out_event| will own the read message upon return, and all other arguments
  // are ignored.
  //
  // If the read message is still serialized, it must have been created by
  // CreateFromChannelMessage() above whenever its bytes were first read from a
  // Channel. In this case, the message will be taken form the port and returned
  // in |*out_event|, if and only iff |read_any_size| is true or both
  // |*num_bytes| and |*num_handles| are sufficiently large to contain the
  // contents of the message. Upon success this returns |MOJO_RESULT_OK|, and
  // updates |*num_bytes| and |*num_handles| with the actual size of the read
  // message.
  //
  // Upon failure this returns any of various error codes detailed by the
  // documentation for MojoReadMessage/MojoReadMessageNew in
  // src/mojo/public/c/system/message_pipe.h.
  static MojoResult ReadMessageEventFromPort(
      NodeController* node_controller,
      const ports::PortRef& port,
      bool read_any_size,
      bool may_discard,
      uint32_t* num_bytes,
      MojoHandle* handles,
      uint32_t* num_handles,
      std::unique_ptr<ports::UserMessageEvent>* out_event);

  // Extracts the serialized Channel::Message from the UserMessageEvent in
  // |event|. |event| must have a serialized UserMessageImpl instance attached.
  // |message_event| is serialized into the front of the message payload before
  // returning.
  static Channel::MessagePtr FinalizeEventMessage(
      std::unique_ptr<ports::UserMessageEvent> event);

  bool HasContext() const { return context_ != 0; }
  uintptr_t ReleaseContext();

  uintptr_t context() const { return context_; }

  bool IsSerialized() const {
    if (HasContext()) {
      DCHECK(!channel_message_);
      return false;
    }

    DCHECK(channel_message_);
    return true;
  }

  void* user_payload() {
    DCHECK(IsSerialized());
    return user_payload_;
  }

  const void* user_payload() const {
    DCHECK(IsSerialized());
    return user_payload_;
  }

  size_t user_payload_size() const {
    DCHECK(IsSerialized());
    return user_payload_size_;
  }

  size_t num_handles() const;

  void set_source_node(const ports::NodeName& name) { source_node_ = name; }
  const ports::NodeName& source_node() const { return source_node_; }

  MojoResult SerializeIfNecessary(ports::UserMessageEvent* message_event);

 private:
  class ReadMessageFilter;

  // Creates an unserialized UserMessageImpl with an associated |context| and
  // |thunks|. If the message is ever going to be routed to another node (see
  // |WillBeRoutedExternally()| below), it will be serialized at that time using
  // operations provided by |thunks|.
  UserMessageImpl(uintptr_t context, const MojoMessageOperationThunks* thunks);

  // Creates a serialized UserMessageImpl backed by an existing Channel::Message
  // object. |header| and |user_payload| must be pointers into
  // |channel_message|'s own storage, and |user_payload_size| is the number of
  // bytes comprising the user message contents at |user_payload|.
  UserMessageImpl(Channel::MessagePtr channel_message,
                  void* header,
                  void* user_payload,
                  size_t user_payload_size);

  // UserMessage:
  bool WillBeRoutedExternally(ports::UserMessageEvent* message_event) override;

  // Unserialized message state.
  uintptr_t context_ = 0;
  const MojoMessageOperationThunks context_thunks_;

  // Serialized message contents. May be null if this is not a serialized
  // message.
  Channel::MessagePtr channel_message_;

  // Only valid if |channel_message_| is non-null. |header_| is the address
  // of the UserMessageImpl's internal MessageHeader structure within the
  // serialized message buffer. |user_payload_| is the address of the first byte
  // after any serialized dispatchers, with the payload comprising the remaining
  // |user_payload_size_| bytes of the message.
  void* header_ = nullptr;
  void* user_payload_ = nullptr;
  size_t user_payload_size_ = 0;

  // The node name from which this message was received, iff it came from
  // out-of-process and the source is known.
  ports::NodeName source_node_ = ports::kInvalidNodeName;

  DISALLOW_COPY_AND_ASSIGN(UserMessageImpl);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_USER_MESSAGE_IMPL_H_
