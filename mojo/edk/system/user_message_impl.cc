// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/user_message_impl.h"

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/node_channel.h"
#include "mojo/edk/system/node_controller.h"
#include "mojo/edk/system/ports/event.h"
#include "mojo/edk/system/ports/message_filter.h"
#include "mojo/edk/system/ports/node.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {
namespace edk {

namespace {

#pragma pack(push, 1)
// Header attached to every message.
struct MessageHeader {
  // The number of serialized dispatchers included in this header.
  uint32_t num_dispatchers;

  // Total size of the header, including serialized dispatcher data.
  uint32_t header_size;
};

// Header for each dispatcher in a message, immediately following the message
// header.
struct DispatcherHeader {
  // The type of the dispatcher, correpsonding to the Dispatcher::Type enum.
  int32_t type;

  // The size of the serialized dispatcher, not including this header.
  uint32_t num_bytes;

  // The number of ports needed to deserialize this dispatcher.
  uint32_t num_ports;

  // The number of platform handles needed to deserialize this dispatcher.
  uint32_t num_platform_handles;
};
#pragma pack(pop)

static_assert(sizeof(MessageHeader) % 8 == 0, "Invalid MessageHeader size.");
static_assert(sizeof(DispatcherHeader) % 8 == 0,
              "Invalid DispatcherHeader size.");

// Creates a new Channel message with sufficient storage for |num_bytes| user
// message payload and all |dispatchers| given.
MojoResult SerializeEventMessage(
    ports::UserMessageEvent* event,
    size_t payload_size,
    const Dispatcher::DispatcherInTransit* dispatchers,
    size_t num_dispatchers,
    Channel::MessagePtr* out_message,
    void** out_header,
    void** out_user_payload) {
  // A structure for tracking information about every Dispatcher that will be
  // serialized into the message. This is NOT part of the message itself.
  struct DispatcherInfo {
    uint32_t num_bytes;
    uint32_t num_ports;
    uint32_t num_handles;
  };

  // This is only the base header size. It will grow as we accumulate the
  // size of serialized state for each dispatcher.
  base::CheckedNumeric<size_t> safe_header_size = num_dispatchers;
  safe_header_size *= sizeof(DispatcherHeader);
  safe_header_size += sizeof(MessageHeader);
  size_t header_size = safe_header_size.ValueOrDie();
  size_t num_ports = 0;
  size_t num_handles = 0;

  std::vector<DispatcherInfo> dispatcher_info(num_dispatchers);
  for (size_t i = 0; i < num_dispatchers; ++i) {
    Dispatcher* d = dispatchers[i].dispatcher.get();
    d->StartSerialize(&dispatcher_info[i].num_bytes,
                      &dispatcher_info[i].num_ports,
                      &dispatcher_info[i].num_handles);
    header_size += dispatcher_info[i].num_bytes;
    num_ports += dispatcher_info[i].num_ports;
    num_handles += dispatcher_info[i].num_handles;
  }

  // We now have enough information to fully allocate the message storage.
  event->ReservePorts(num_ports);
  const size_t event_size = event->GetSerializedSize();
  const size_t total_size = event_size + header_size + payload_size;
  void* data;
  Channel::MessagePtr message =
      NodeChannel::CreateEventMessage(total_size, &data, num_handles);
  auto* header = reinterpret_cast<MessageHeader*>(static_cast<uint8_t*>(data) +
                                                  event_size);

  // Populate the message header with information about serialized dispatchers.
  // The front of the message is always a MessageHeader followed by a
  // DispatcherHeader for each dispatcher to be sent.
  DispatcherHeader* dispatcher_headers =
      reinterpret_cast<DispatcherHeader*>(header + 1);

  // Serialized dispatcher state immediately follows the series of
  // DispatcherHeaders.
  char* dispatcher_data =
      reinterpret_cast<char*>(dispatcher_headers + num_dispatchers);

  header->num_dispatchers =
      base::CheckedNumeric<uint32_t>(num_dispatchers).ValueOrDie();

  // |header_size| is the total number of bytes preceding the message payload,
  // including all dispatcher headers and serialized dispatcher state.
  if (!base::IsValueInRangeForNumericType<uint32_t>(header_size))
    return MOJO_RESULT_OUT_OF_RANGE;

  header->header_size = static_cast<uint32_t>(header_size);

  if (num_dispatchers > 0) {
    ScopedPlatformHandleVectorPtr handles(
        new PlatformHandleVector(num_handles));
    size_t port_index = 0;
    size_t handle_index = 0;
    bool fail = false;
    for (size_t i = 0; i < num_dispatchers; ++i) {
      Dispatcher* d = dispatchers[i].dispatcher.get();
      DispatcherHeader* dh = &dispatcher_headers[i];
      const DispatcherInfo& info = dispatcher_info[i];

      // Fill in the header for this dispatcher.
      dh->type = static_cast<int32_t>(d->GetType());
      dh->num_bytes = info.num_bytes;
      dh->num_ports = info.num_ports;
      dh->num_platform_handles = info.num_handles;

      // Fill in serialized state, ports, and platform handles. We'll cancel
      // the send if the dispatcher implementation rejects for some reason.
      if (!d->EndSerialize(static_cast<void*>(dispatcher_data),
                           event->ports() + port_index,
                           handles->data() + handle_index)) {
        fail = true;
        break;
      }

      dispatcher_data += info.num_bytes;
      port_index += info.num_ports;
      handle_index += info.num_handles;
    }

    if (fail) {
      // Release any platform handles we've accumulated. Their dispatchers
      // retain ownership when message creation fails, so these are not actually
      // leaking.
      handles->clear();
      return MOJO_RESULT_INVALID_ARGUMENT;
    }

    // Take ownership of all the handles and move them into message storage.
    message->SetHandles(std::move(handles));
  }

  *out_message = std::move(message);
  *out_header = header;
  *out_user_payload = reinterpret_cast<uint8_t*>(header) + header_size;
  return MOJO_RESULT_OK;
}

}  // namespace

// A MessageFilter used by UserMessageImpl::ReadMessageEventFromPort to
// determine whether a message should actually be consumed yet.
class UserMessageImpl::ReadMessageFilter : public ports::MessageFilter {
 public:
  // Creates a new ReadMessageFilter which captures and potentially modifies
  // various (unowned) local state within
  // UserMessageImpl::ReadMessageEventFromPort.
  ReadMessageFilter(bool read_any_size,
                    bool may_discard,
                    uint32_t* num_bytes,
                    uint32_t* num_handles,
                    bool* no_space,
                    bool* invalid_message)
      : read_any_size_(read_any_size),
        may_discard_(may_discard),
        num_bytes_(num_bytes),
        num_handles_(num_handles),
        no_space_(no_space),
        invalid_message_(invalid_message) {}

  ~ReadMessageFilter() override {}

  // ports::MessageFilter:
  bool Match(const ports::UserMessageEvent& event) override {
    const auto* message = event.GetMessage<UserMessageImpl>();
    if (!message->IsSerialized()) {
      // Not a serialized message, so there's nothing to validate or filter
      // against. We only ensure that the caller expected a message object and
      // not a specific serialized buffer size.
      if (!read_any_size_) {
        *invalid_message_ = true;
        return false;
      }
      return true;
    }

    // All messages which reach this filter have already had a basic level of
    // validation applied by UserMessageImpl::CreateFromChannelMessage()
    // so we know there is at least well-formed header.
    DCHECK(message->header_);
    auto* header = static_cast<MessageHeader*>(message->header_);

    uint32_t bytes_to_read = 0;
    base::CheckedNumeric<uint32_t> checked_bytes_available =
        message->user_payload_size();
    if (!checked_bytes_available.IsValid()) {
      *invalid_message_ = true;
      return true;
    }
    const uint32_t bytes_available = checked_bytes_available.ValueOrDie();
    if (num_bytes_) {
      bytes_to_read = std::min(*num_bytes_, bytes_available);
      *num_bytes_ = bytes_available;
    }

    uint32_t handles_to_read = 0;
    uint32_t handles_available = header->num_dispatchers;
    if (num_handles_) {
      handles_to_read = std::min(*num_handles_, handles_available);
      *num_handles_ = handles_available;
    }

    if (handles_to_read < handles_available ||
        (!read_any_size_ && bytes_to_read < bytes_available)) {
      *no_space_ = true;
      return may_discard_;
    }

    return true;
  }

 private:
  const bool read_any_size_;
  const bool may_discard_;
  uint32_t* const num_bytes_;
  uint32_t* const num_handles_;
  bool* const no_space_;
  bool* const invalid_message_;

  DISALLOW_COPY_AND_ASSIGN(ReadMessageFilter);
};

// static
const ports::UserMessage::TypeInfo UserMessageImpl::kUserMessageTypeInfo = {};

UserMessageImpl::~UserMessageImpl() {
  if (HasContext())
    context_thunks_.destroy(context_);
}

// static
std::unique_ptr<ports::UserMessageEvent>
UserMessageImpl::CreateEventForNewMessageWithContext(
    uintptr_t context,
    const MojoMessageOperationThunks* thunks) {
  auto message_event = base::MakeUnique<ports::UserMessageEvent>(0);
  message_event->AttachMessage(
      base::WrapUnique(new UserMessageImpl(context, thunks)));
  return message_event;
}

// static
MojoResult UserMessageImpl::CreateEventForNewSerializedMessage(
    uint32_t num_bytes,
    const Dispatcher::DispatcherInTransit* dispatchers,
    uint32_t num_dispatchers,
    std::unique_ptr<ports::UserMessageEvent>* out_event) {
  Channel::MessagePtr channel_message;
  void* header = nullptr;
  void* user_payload = nullptr;
  auto event = base::MakeUnique<ports::UserMessageEvent>(0);
  MojoResult rv = SerializeEventMessage(event.get(), num_bytes, dispatchers,
                                        num_dispatchers, &channel_message,
                                        &header, &user_payload);
  if (rv != MOJO_RESULT_OK)
    return rv;
  event->AttachMessage(base::WrapUnique(new UserMessageImpl(
      std::move(channel_message), header, user_payload, num_bytes)));
  *out_event = std::move(event);
  return MOJO_RESULT_OK;
}

// static
std::unique_ptr<UserMessageImpl> UserMessageImpl::CreateFromChannelMessage(
    Channel::MessagePtr channel_message,
    void* payload,
    size_t payload_size) {
  DCHECK(channel_message);
  if (payload_size < sizeof(MessageHeader))
    return nullptr;

  auto* header = static_cast<MessageHeader*>(payload);
  const size_t header_size = header->header_size;
  if (header_size > payload_size)
    return nullptr;

  void* user_payload = static_cast<uint8_t*>(payload) + header_size;
  const size_t user_payload_size = payload_size - header_size;
  return base::WrapUnique(new UserMessageImpl(
      std::move(channel_message), header, user_payload, user_payload_size));
}

// static
MojoResult UserMessageImpl::ReadMessageEventFromPort(
    NodeController* node_controller,
    const ports::PortRef& port,
    bool read_any_size,
    bool may_discard,
    uint32_t* num_bytes,
    MojoHandle* handles,
    uint32_t* num_handles,
    std::unique_ptr<ports::UserMessageEvent>* out_event) {
  bool no_space = false;
  bool invalid_message = false;
  ReadMessageFilter filter(read_any_size, may_discard, num_bytes, num_handles,
                           &no_space, &invalid_message);
  std::unique_ptr<ports::UserMessageEvent> message_event;
  int rv = node_controller->node()->GetMessage(port, &message_event, &filter);
  if (invalid_message)
    return MOJO_RESULT_NOT_FOUND;

  if (rv != ports::OK && rv != ports::ERROR_PORT_PEER_CLOSED) {
    if (rv == ports::ERROR_PORT_UNKNOWN ||
        rv == ports::ERROR_PORT_STATE_UNEXPECTED)
      return MOJO_RESULT_INVALID_ARGUMENT;

    NOTREACHED();
    return MOJO_RESULT_UNKNOWN;  // TODO: Add a better error code here?
  }

  if (no_space) {
    // |*num_handles| (and/or |*num_bytes| if |read_any_size| is false) wasn't
    // sufficient to hold this message's data. The message will still be in
    // queue unless MOJO_READ_MESSAGE_FLAG_MAY_DISCARD was set.
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  if (!message_event) {
    // No message was available in queue.
    if (rv == ports::OK)
      return MOJO_RESULT_SHOULD_WAIT;
    // Peer is closed and there are no more messages to read.
    DCHECK_EQ(rv, ports::ERROR_PORT_PEER_CLOSED);
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  // Alright! We have a message and the caller has provided sufficient storage
  // in which to receive it, if applicable.

  auto* message = message_event->GetMessage<UserMessageImpl>();
  if (message->HasContext()) {
    // Not a serialized message, so there's no more work to do.
    *out_event = std::move(message_event);
    return MOJO_RESULT_OK;
  }

  DCHECK(message->IsSerialized());

  const MessageHeader* header =
      static_cast<const MessageHeader*>(message->header_);
  const DispatcherHeader* dispatcher_headers =
      reinterpret_cast<const DispatcherHeader*>(header + 1);

  if (header->num_dispatchers > std::numeric_limits<uint16_t>::max())
    return MOJO_RESULT_UNKNOWN;

  // Deserialize dispatchers.
  if (header->num_dispatchers > 0) {
    DCHECK(handles);
    std::vector<Dispatcher::DispatcherInTransit> dispatchers(
        header->num_dispatchers);

    size_t data_payload_index =
        sizeof(MessageHeader) +
        header->num_dispatchers * sizeof(DispatcherHeader);
    if (data_payload_index > header->header_size)
      return MOJO_RESULT_UNKNOWN;
    const char* dispatcher_data = reinterpret_cast<const char*>(
        dispatcher_headers + header->num_dispatchers);
    size_t port_index = 0;
    size_t platform_handle_index = 0;
    ScopedPlatformHandleVectorPtr msg_handles =
        message->channel_message_->TakeHandles();
    const size_t num_msg_handles = msg_handles ? msg_handles->size() : 0;
    for (size_t i = 0; i < header->num_dispatchers; ++i) {
      const DispatcherHeader& dh = dispatcher_headers[i];
      auto type = static_cast<Dispatcher::Type>(dh.type);

      base::CheckedNumeric<size_t> next_payload_index = data_payload_index;
      next_payload_index += dh.num_bytes;
      if (!next_payload_index.IsValid() ||
          header->header_size < next_payload_index.ValueOrDie()) {
        return MOJO_RESULT_UNKNOWN;
      }

      base::CheckedNumeric<size_t> next_port_index = port_index;
      next_port_index += dh.num_ports;
      if (!next_port_index.IsValid() ||
          message_event->num_ports() < next_port_index.ValueOrDie()) {
        return MOJO_RESULT_UNKNOWN;
      }

      base::CheckedNumeric<size_t> next_platform_handle_index =
          platform_handle_index;
      next_platform_handle_index += dh.num_platform_handles;
      if (!next_platform_handle_index.IsValid() ||
          num_msg_handles < next_platform_handle_index.ValueOrDie()) {
        return MOJO_RESULT_UNKNOWN;
      }

      PlatformHandle* out_handles =
          num_msg_handles ? msg_handles->data() + platform_handle_index
                          : nullptr;
      dispatchers[i].dispatcher = Dispatcher::Deserialize(
          type, dispatcher_data, dh.num_bytes,
          message_event->ports() + port_index, dh.num_ports, out_handles,
          dh.num_platform_handles);
      if (!dispatchers[i].dispatcher)
        return MOJO_RESULT_UNKNOWN;

      dispatcher_data += dh.num_bytes;
      data_payload_index = next_payload_index.ValueOrDie();
      port_index = next_port_index.ValueOrDie();
      platform_handle_index = next_platform_handle_index.ValueOrDie();
    }

    if (!node_controller->core()->AddDispatchersFromTransit(dispatchers,
                                                            handles)) {
      return MOJO_RESULT_UNKNOWN;
    }
  }

  *out_event = std::move(message_event);
  return MOJO_RESULT_OK;
}

// static
Channel::MessagePtr UserMessageImpl::FinalizeEventMessage(
    std::unique_ptr<ports::UserMessageEvent> message_event) {
  auto* message = message_event->GetMessage<UserMessageImpl>();
  DCHECK(message->IsSerialized());

  Channel::MessagePtr channel_message = std::move(message->channel_message_);
  DCHECK(channel_message);
  message->user_payload_ = nullptr;
  message->user_payload_size_ = 0;

  // Serialize the UserMessageEvent into the front of the message payload where
  // there is already space reserved for it.
  void* data;
  size_t size;
  NodeChannel::GetEventMessageData(channel_message.get(), &data, &size);
  message_event->Serialize(data);
  return channel_message;
}

uintptr_t UserMessageImpl::ReleaseContext() {
  uintptr_t context = context_;
  context_ = 0;
  return context;
}

size_t UserMessageImpl::num_handles() const {
  DCHECK(IsSerialized());
  DCHECK(header_);
  return static_cast<const MessageHeader*>(header_)->num_dispatchers;
}

MojoResult UserMessageImpl::SerializeIfNecessary(
    ports::UserMessageEvent* message_event) {
  if (IsSerialized())
    return MOJO_RESULT_FAILED_PRECONDITION;

  DCHECK(HasContext());
  size_t num_bytes = 0;
  size_t num_handles = 0;
  context_thunks_.get_serialized_size(context_, &num_bytes, &num_handles);

  std::vector<ScopedHandle> handles(num_handles);
  std::vector<Dispatcher::DispatcherInTransit> dispatchers;
  if (num_handles > 0) {
    auto* raw_handles = reinterpret_cast<MojoHandle*>(handles.data());
    context_thunks_.serialize_handles(context_, raw_handles);
    MojoResult acquire_result = internal::g_core->AcquireDispatchersForTransit(
        raw_handles, num_handles, &dispatchers);
    if (acquire_result != MOJO_RESULT_OK)
      return acquire_result;
  }

  Channel::MessagePtr channel_message;
  MojoResult rv = SerializeEventMessage(
      message_event, num_bytes, dispatchers.data(), num_handles,
      &channel_message, &header_, &user_payload_);
  if (num_handles > 0) {
    internal::g_core->ReleaseDispatchersForTransit(dispatchers,
                                                   rv == MOJO_RESULT_OK);
  }

  if (rv != MOJO_RESULT_OK)
    return MOJO_RESULT_ABORTED;

  // The handles are now owned by the message event. Release them here.
  for (auto& handle : handles)
    ignore_result(handle.release());

  context_thunks_.serialize_payload(context_, user_payload_);
  user_payload_size_ = num_bytes;
  channel_message_ = std::move(channel_message);

  context_thunks_.destroy(context_);
  context_ = 0;
  return MOJO_RESULT_OK;
}

UserMessageImpl::UserMessageImpl(uintptr_t context,
                                 const MojoMessageOperationThunks* thunks)
    : ports::UserMessage(&kUserMessageTypeInfo),
      context_(context),
      context_thunks_(*thunks) {}

UserMessageImpl::UserMessageImpl(Channel::MessagePtr channel_message,
                                 void* header,
                                 void* user_payload,
                                 size_t user_payload_size)
    : ports::UserMessage(&kUserMessageTypeInfo),
      context_thunks_({}),
      channel_message_(std::move(channel_message)),
      header_(header),
      user_payload_(user_payload),
      user_payload_size_(user_payload_size) {}

bool UserMessageImpl::WillBeRoutedExternally(
    ports::UserMessageEvent* message_event) {
  MojoResult result = SerializeIfNecessary(message_event);
  return result == MOJO_RESULT_OK || result == MOJO_RESULT_FAILED_PRECONDITION;
}

}  // namespace edk
}  // namespace mojo
