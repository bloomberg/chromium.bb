// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_USER_MESSAGE_IMPL_H_
#define MOJO_EDK_SYSTEM_USER_MESSAGE_IMPL_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/optional.h"
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
#include "mojo/public/cpp/system/handle.h"

namespace mojo {
namespace edk {

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

  // Determines how ExtractSerializedHandles should behave when it encounters an
  // unrecoverable serialized handle.
  enum ExtractBadHandlePolicy {
    // Continue extracting handles upon encountering a bad handle. The bad
    // handle will be extracted with an invalid handle value.
    kSkip,

    // Abort the extraction process, leaving any valid serialized handles still
    // in the message.
    kAbort,
  };

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
      ports::UserMessageEvent* message_event,
      Channel::MessagePtr channel_message,
      void* payload,
      size_t payload_size);

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

    return !!channel_message_;
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

  // If this message is not already serialized, this serializes it.
  MojoResult SerializeIfNecessary();

  // Extracts handles from this (serialized) message.
  //
  // Returns |MOJO_RESULT_OK|
  // if sucessful, |MOJO_RESULT_FAILED_PRECONDITION| if this isn't a serialized
  // message, |MOJO_RESULT_NOT_FOUND| if all serialized handles have already
  // been extracted, or |MOJO_RESULT_ABORTED| if one or more handles failed
  // extraction.
  //
  // On success, |handles| is populated with |num_handles()| extracted handles,
  // whose ownership is thereby transferred to the caller.
  MojoResult ExtractSerializedHandles(ExtractBadHandlePolicy bad_handle_policy,
                                      MojoHandle* handles);

 private:
  // Creates an unserialized UserMessageImpl with an associated |context| and
  // |thunks|. If the message is ever going to be routed to another node (see
  // |WillBeRoutedExternally()| below), it will be serialized at that time using
  // operations provided by |thunks|.
  UserMessageImpl(ports::UserMessageEvent* message_event,
                  uintptr_t context,
                  const MojoMessageOperationThunks* thunks);

  // Creates a serialized UserMessageImpl backed by an existing Channel::Message
  // object. |header| and |user_payload| must be pointers into
  // |channel_message|'s own storage, and |user_payload_size| is the number of
  // bytes comprising the user message contents at |user_payload|.
  UserMessageImpl(ports::UserMessageEvent* message_event,
                  Channel::MessagePtr channel_message,
                  void* header,
                  void* user_payload,
                  size_t user_payload_size);

  // UserMessage:
  bool WillBeRoutedExternally() override;

  // The event which owns this serialized message. Not owned.
  ports::UserMessageEvent* const message_event_;

  // Unserialized message state.
  uintptr_t context_ = 0;
  base::Optional<MojoMessageOperationThunks> context_thunks_;

  // Serialized message contents. May be null if this is not a serialized
  // message.
  Channel::MessagePtr channel_message_;

  // Indicates whether any handles serialized within |channel_message_| have
  // yet to be extracted.
  bool has_serialized_handles_ = false;

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
