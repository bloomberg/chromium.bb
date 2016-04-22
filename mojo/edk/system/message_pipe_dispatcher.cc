// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/message_pipe_dispatcher.h"

#include <limits>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/node_controller.h"
#include "mojo/edk/system/ports_message.h"
#include "mojo/edk/system/request_context.h"

namespace mojo {
namespace edk {

namespace {

#pragma pack(push, 1)

// Header attached to every message sent over a message pipe.
struct MessageHeader {
  // The number of serialized dispatchers included in this header.
  uint32_t num_dispatchers;

  // Total size of the header, including serialized dispatcher data.
  uint32_t header_size;
};

static_assert(sizeof(MessageHeader) % 8 == 0, "Invalid MessageHeader size.");

// Header for each dispatcher, immediately following the message header.
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

static_assert(sizeof(DispatcherHeader) % 8 == 0,
              "Invalid DispatcherHeader size.");

struct SerializedState {
  uint64_t pipe_id;
  int8_t endpoint;
  char padding[7];
};

static_assert(sizeof(SerializedState) % 8 == 0,
              "Invalid SerializedState size.");

#pragma pack(pop)

}  // namespace

// A PortObserver which forwards to a MessagePipeDispatcher. This owns a
// reference to the MPD to ensure it lives as long as the observed port.
class MessagePipeDispatcher::PortObserverThunk
    : public NodeController::PortObserver {
 public:
  explicit PortObserverThunk(scoped_refptr<MessagePipeDispatcher> dispatcher)
      : dispatcher_(dispatcher) {}

 private:
  ~PortObserverThunk() override {}

  // NodeController::PortObserver:
  void OnPortStatusChanged() override { dispatcher_->OnPortStatusChanged(); }

  scoped_refptr<MessagePipeDispatcher> dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(PortObserverThunk);
};

MessagePipeDispatcher::MessagePipeDispatcher(NodeController* node_controller,
                                             const ports::PortRef& port,
                                             uint64_t pipe_id,
                                             int endpoint)
    : node_controller_(node_controller),
      port_(port),
      pipe_id_(pipe_id),
      endpoint_(endpoint) {
  DVLOG(2) << "Creating new MessagePipeDispatcher for port " << port.name()
           << " [pipe_id=" << pipe_id << "; endpoint=" << endpoint << "]";

  node_controller_->SetPortObserver(
      port_,
      make_scoped_refptr(new PortObserverThunk(this)));
}

bool MessagePipeDispatcher::Fuse(MessagePipeDispatcher* other) {
  node_controller_->SetPortObserver(port_, nullptr);
  node_controller_->SetPortObserver(other->port_, nullptr);

  ports::PortRef port0;
  {
    base::AutoLock lock(signal_lock_);
    port0 = port_;
    port_closed_.Set(true);
    awakables_.CancelAll();
  }

  ports::PortRef port1;
  {
    base::AutoLock lock(other->signal_lock_);
    port1 = other->port_;
    other->port_closed_.Set(true);
    other->awakables_.CancelAll();
  }

  // Both ports are always closed by this call.
  int rv = node_controller_->MergeLocalPorts(port0, port1);
  return rv == ports::OK;
}

Dispatcher::Type MessagePipeDispatcher::GetType() const {
  return Type::MESSAGE_PIPE;
}

MojoResult MessagePipeDispatcher::Close() {
  base::AutoLock lock(signal_lock_);
  DVLOG(1) << "Closing message pipe " << pipe_id_ << " endpoint " << endpoint_
           << " [port=" << port_.name() << "]";
  return CloseNoLock();
}

MojoResult MessagePipeDispatcher::Watch(MojoHandleSignals signals,
                                        const Watcher::WatchCallback& callback,
                                        uintptr_t context) {
  base::AutoLock lock(signal_lock_);

  if (port_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return awakables_.AddWatcher(
      signals, callback, context, GetHandleSignalsStateNoLock());
}

MojoResult MessagePipeDispatcher::CancelWatch(uintptr_t context) {
  base::AutoLock lock(signal_lock_);

  if (port_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return awakables_.RemoveWatcher(context);
}

MojoResult MessagePipeDispatcher::WriteMessage(
    const void* bytes,
    uint32_t num_bytes,
    const DispatcherInTransit* dispatchers,
    uint32_t num_dispatchers,
    MojoWriteMessageFlags flags) {


  if (port_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  // A structure for retaining information about every Dispatcher we're about
  // to send. This information is collected by calling StartSerialize() on
  // each dispatcher in sequence.
  struct DispatcherInfo {
    uint32_t num_bytes;
    uint32_t num_ports;
    uint32_t num_handles;
  };

  // This is only the base header size. It will grow as we accumulate the
  // size of serialized state for each dispatcher.
  size_t header_size = sizeof(MessageHeader) +
      num_dispatchers * sizeof(DispatcherHeader);

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
  std::unique_ptr<PortsMessage> message = PortsMessage::NewUserMessage(
      header_size + num_bytes, num_ports, num_handles);
  DCHECK(message);

  // Populate the message header with information about serialized dispatchers.
  //
  // The front of the message is always a MessageHeader followed by a
  // DispatcherHeader for each dispatcher to be sent.
  MessageHeader* header =
      static_cast<MessageHeader*>(message->mutable_payload_bytes());
  DispatcherHeader* dispatcher_headers =
      reinterpret_cast<DispatcherHeader*>(header + 1);

  // Serialized dispatcher state immediately follows the series of
  // DispatcherHeaders.
  char* dispatcher_data =
      reinterpret_cast<char*>(dispatcher_headers + num_dispatchers);

  header->num_dispatchers = num_dispatchers;

  // |header_size| is the total number of bytes preceding the message payload,
  // including all dispatcher headers and serialized dispatcher state.
  DCHECK_LE(header_size, std::numeric_limits<uint32_t>::max());
  header->header_size = static_cast<uint32_t>(header_size);

  bool cancel_transit = false;
  if (num_dispatchers > 0) {
    ScopedPlatformHandleVectorPtr handles(
        new PlatformHandleVector(num_handles));
    size_t port_index = 0;
    size_t handle_index = 0;
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
                           message->mutable_ports() + port_index,
                           handles->data() + handle_index)) {
        cancel_transit = true;
        break;
      }

      dispatcher_data += info.num_bytes;
      port_index += info.num_ports;
      handle_index += info.num_handles;
    }

    if (!cancel_transit) {
      // Take ownership of all the handles and move them into message storage.
      message->SetHandles(std::move(handles));
    } else {
      // Release any platform handles we've accumulated. Their dispatchers
      // retain ownership when transit is canceled, so these are not actually
      // leaking.
      handles->clear();
    }
  }

  MojoResult result = MOJO_RESULT_OK;
  if (!cancel_transit) {
    // Copy the message body.
    void* message_body = static_cast<void*>(
        static_cast<char*>(message->mutable_payload_bytes()) + header_size);
    memcpy(message_body, bytes, num_bytes);

    int rv = node_controller_->SendMessage(port_, &message);

    DVLOG(1) << "Sent message on pipe " << pipe_id_ << " endpoint " << endpoint_
             << " [port=" << port_.name() << "; rv=" << rv
             << "; num_bytes=" << num_bytes << "]";

    if (rv != ports::OK) {
      if (rv == ports::ERROR_PORT_UNKNOWN ||
          rv == ports::ERROR_PORT_STATE_UNEXPECTED ||
          rv == ports::ERROR_PORT_CANNOT_SEND_PEER) {
        result = MOJO_RESULT_INVALID_ARGUMENT;
      } else if (rv == ports::ERROR_PORT_PEER_CLOSED) {
        base::AutoLock lock(signal_lock_);
        awakables_.AwakeForStateChange(GetHandleSignalsStateNoLock());
        result = MOJO_RESULT_FAILED_PRECONDITION;
      } else {
        NOTREACHED();
        result = MOJO_RESULT_UNKNOWN;
      }
      cancel_transit = true;
    } else {
      DCHECK(!message);
    }
  }

  if (cancel_transit) {
    // We ended up not sending the message. Release all the platform handles.
    // Their dipatchers retain ownership when transit is canceled, so these are
    // not actually leaking.
    DCHECK(message);
    Channel::MessagePtr m = message->TakeChannelMessage();
    ScopedPlatformHandleVectorPtr handles = m->TakeHandles();
    if (handles)
      handles->clear();
  }

  return result;
}

MojoResult MessagePipeDispatcher::ReadMessage(void* bytes,
                                              uint32_t* num_bytes,
                                              MojoHandle* handles,
                                              uint32_t* num_handles,
                                              MojoReadMessageFlags flags) {
  // We can't read from a port that's closed or in transit!
  if (port_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  bool no_space = false;
  bool may_discard = flags & MOJO_READ_MESSAGE_FLAG_MAY_DISCARD;

  // Ensure the provided buffers are large enough to hold the next message.
  // GetMessageIf provides an atomic way to test the next message without
  // committing to removing it from the port's underlying message queue until
  // we are sure we can consume it.

  ports::ScopedMessage ports_message;
  int rv = node_controller_->node()->GetMessageIf(
      port_,
      [num_bytes, num_handles, &no_space, &may_discard](
          const ports::Message& next_message) {
        const PortsMessage& message =
            static_cast<const PortsMessage&>(next_message);
        DCHECK_GE(message.num_payload_bytes(), sizeof(MessageHeader));

        const MessageHeader* header =
            static_cast<const MessageHeader*>(message.payload_bytes());
        DCHECK_LE(header->header_size, message.num_payload_bytes());

        uint32_t bytes_to_read = 0;
        uint32_t bytes_available =
            static_cast<uint32_t>(message.num_payload_bytes()) -
            header->header_size;
        if (num_bytes) {
          bytes_to_read = std::min(*num_bytes, bytes_available);
          *num_bytes = bytes_available;
        }

        uint32_t handles_to_read = 0;
        uint32_t handles_available = header->num_dispatchers;
        if (num_handles) {
          handles_to_read = std::min(*num_handles, handles_available);
          *num_handles = handles_available;
        }

        if (bytes_to_read < bytes_available ||
            handles_to_read < handles_available) {
          no_space = true;
          return may_discard;
        }

        return true;
      },
      &ports_message);

  if (rv != ports::OK && rv != ports::ERROR_PORT_PEER_CLOSED) {
    if (rv == ports::ERROR_PORT_UNKNOWN ||
        rv == ports::ERROR_PORT_STATE_UNEXPECTED)
      return MOJO_RESULT_INVALID_ARGUMENT;

    NOTREACHED();
    return MOJO_RESULT_UNKNOWN;  // TODO: Add a better error code here?
  }

  if (no_space) {
    // Either |*num_bytes| or |*num_handles| wasn't sufficient to hold this
    // message's data. The message will still be in queue unless
    // MOJO_READ_MESSAGE_FLAG_MAY_DISCARD was set.
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  if (!ports_message) {
    // No message was available in queue.

    if (rv == ports::OK)
      return MOJO_RESULT_SHOULD_WAIT;

    // Peer is closed and there are no more messages to read.
    DCHECK_EQ(rv, ports::ERROR_PORT_PEER_CLOSED);
    base::AutoLock lock(signal_lock_);
    awakables_.AwakeForStateChange(GetHandleSignalsStateNoLock());
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  // Alright! We have a message and the caller has provided sufficient storage
  // in which to receive it.

  std::unique_ptr<PortsMessage> message(
      static_cast<PortsMessage*>(ports_message.release()));

  const MessageHeader* header =
      static_cast<const MessageHeader*>(message->payload_bytes());
  const DispatcherHeader* dispatcher_headers =
      reinterpret_cast<const DispatcherHeader*>(header + 1);

  const char* dispatcher_data = reinterpret_cast<const char*>(
      dispatcher_headers + header->num_dispatchers);

  // Deserialize dispatchers.
  if (header->num_dispatchers > 0) {
    CHECK(handles);
    std::vector<DispatcherInTransit> dispatchers(header->num_dispatchers);
    size_t data_payload_index = sizeof(MessageHeader) +
        header->num_dispatchers * sizeof(DispatcherHeader);
    size_t port_index = 0;
    size_t platform_handle_index = 0;
    for (size_t i = 0; i < header->num_dispatchers; ++i) {
      const DispatcherHeader& dh = dispatcher_headers[i];
      Type type = static_cast<Type>(dh.type);

      DCHECK_GE(message->num_payload_bytes(),
                data_payload_index + dh.num_bytes);
      DCHECK_GE(message->num_ports(),
                port_index + dh.num_ports);
      DCHECK_GE(message->num_handles(),
                platform_handle_index + dh.num_platform_handles);

      PlatformHandle* out_handles =
          message->num_handles() ? message->handles() + platform_handle_index
                                 : nullptr;
      dispatchers[i].dispatcher = Dispatcher::Deserialize(
          type, dispatcher_data, dh.num_bytes, message->ports() + port_index,
          dh.num_ports, out_handles, dh.num_platform_handles);
      if (!dispatchers[i].dispatcher)
        return MOJO_RESULT_UNKNOWN;

      dispatcher_data += dh.num_bytes;
      data_payload_index += dh.num_bytes;
      port_index += dh.num_ports;
      platform_handle_index += dh.num_platform_handles;
    }

    if (!node_controller_->core()->AddDispatchersFromTransit(dispatchers,
                                                             handles))
      return MOJO_RESULT_UNKNOWN;
  }

  // Copy message bytes.
  DCHECK_GE(message->num_payload_bytes(), header->header_size);
  const char* payload = reinterpret_cast<const char*>(message->payload_bytes());
  memcpy(bytes, payload + header->header_size,
         message->num_payload_bytes() - header->header_size);

  return MOJO_RESULT_OK;
}

HandleSignalsState
MessagePipeDispatcher::GetHandleSignalsState() const {
  base::AutoLock lock(signal_lock_);
  return GetHandleSignalsStateNoLock();
}

MojoResult MessagePipeDispatcher::AddAwakable(
    Awakable* awakable,
    MojoHandleSignals signals,
    uintptr_t context,
    HandleSignalsState* signals_state) {
  base::AutoLock lock(signal_lock_);

  if (port_closed_ || in_transit_) {
    if (signals_state)
      *signals_state = HandleSignalsState();
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  HandleSignalsState state = GetHandleSignalsStateNoLock();

  DVLOG(2) << "Getting signal state for pipe " << pipe_id_ << " endpoint "
           << endpoint_ << " [awakable=" << awakable << "; port="
           << port_.name() << "; signals=" << signals << "; satisfied="
           << state.satisfied_signals << "; satisfiable="
           << state.satisfiable_signals << "]";

  if (state.satisfies(signals)) {
    if (signals_state)
      *signals_state = state;
    DVLOG(2) << "Signals already set for " << port_.name();
    return MOJO_RESULT_ALREADY_EXISTS;
  }
  if (!state.can_satisfy(signals)) {
    if (signals_state)
      *signals_state = state;
    DVLOG(2) << "Signals impossible to satisfy for " << port_.name();
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  DVLOG(2) << "Adding awakable to pipe " << pipe_id_ << " endpoint "
           << endpoint_ << " [awakable=" << awakable << "; port="
           << port_.name() << "; signals=" << signals << "]";

  awakables_.Add(awakable, signals, context);
  return MOJO_RESULT_OK;
}

void MessagePipeDispatcher::RemoveAwakable(Awakable* awakable,
                                           HandleSignalsState* signals_state) {
  base::AutoLock lock(signal_lock_);
  if (port_closed_ || in_transit_) {
    if (signals_state)
      *signals_state = HandleSignalsState();
  } else if (signals_state) {
    *signals_state = GetHandleSignalsStateNoLock();
  }

  DVLOG(2) << "Removing awakable from pipe " << pipe_id_ << " endpoint "
           << endpoint_ << " [awakable=" << awakable << "; port="
           << port_.name() << "]";

  awakables_.Remove(awakable);
}

void MessagePipeDispatcher::StartSerialize(uint32_t* num_bytes,
                                           uint32_t* num_ports,
                                           uint32_t* num_handles) {
  *num_bytes = static_cast<uint32_t>(sizeof(SerializedState));
  *num_ports = 1;
  *num_handles = 0;
}

bool MessagePipeDispatcher::EndSerialize(void* destination,
                                         ports::PortName* ports,
                                         PlatformHandle* handles) {
  SerializedState* state = static_cast<SerializedState*>(destination);
  state->pipe_id = pipe_id_;
  state->endpoint = static_cast<int8_t>(endpoint_);
  memset(state->padding, 0, sizeof(state->padding));
  ports[0] = port_.name();
  return true;
}

bool MessagePipeDispatcher::BeginTransit() {
  base::AutoLock lock(signal_lock_);
  if (in_transit_ || port_closed_)
    return false;
  in_transit_.Set(true);
  return in_transit_;
}

void MessagePipeDispatcher::CompleteTransitAndClose() {
  node_controller_->SetPortObserver(port_, nullptr);

  base::AutoLock lock(signal_lock_);
  port_transferred_ = true;
  in_transit_.Set(false);
  CloseNoLock();
}

void MessagePipeDispatcher::CancelTransit() {
  base::AutoLock lock(signal_lock_);
  in_transit_.Set(false);

  // Something may have happened while we were waiting for potential transit.
  awakables_.AwakeForStateChange(GetHandleSignalsStateNoLock());
}

// static
scoped_refptr<Dispatcher> MessagePipeDispatcher::Deserialize(
    const void* data,
    size_t num_bytes,
    const ports::PortName* ports,
    size_t num_ports,
    PlatformHandle* handles,
    size_t num_handles) {
  if (num_ports != 1 || num_handles || num_bytes != sizeof(SerializedState))
    return nullptr;

  const SerializedState* state = static_cast<const SerializedState*>(data);

  ports::PortRef port;
  CHECK_EQ(
      ports::OK,
      internal::g_core->GetNodeController()->node()->GetPort(ports[0], &port));

  return new MessagePipeDispatcher(internal::g_core->GetNodeController(), port,
                                   state->pipe_id, state->endpoint);
}

MessagePipeDispatcher::~MessagePipeDispatcher() {
  DCHECK(port_closed_ && !in_transit_);
}

MojoResult MessagePipeDispatcher::CloseNoLock() {
  signal_lock_.AssertAcquired();
  if (port_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  port_closed_.Set(true);
  awakables_.CancelAll();

  if (!port_transferred_) {
    base::AutoUnlock unlock(signal_lock_);
    node_controller_->ClosePort(port_);
  }

  return MOJO_RESULT_OK;
}

HandleSignalsState MessagePipeDispatcher::GetHandleSignalsStateNoLock() const {
  HandleSignalsState rv;

  ports::PortStatus port_status;
  if (node_controller_->node()->GetStatus(port_, &port_status) != ports::OK) {
    CHECK(in_transit_ || port_transferred_ || port_closed_);
    return HandleSignalsState();
  }

  if (port_status.has_messages) {
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_READABLE;
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  }
  if (port_status.receiving_messages)
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  if (!port_status.peer_closed) {
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  } else {
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  }
  rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  return rv;
}

void MessagePipeDispatcher::OnPortStatusChanged() {
  DCHECK(RequestContext::current());

  base::AutoLock lock(signal_lock_);

  // We stop observing our port as soon as it's transferred, but this can race
  // with events which are raised right before that happens. This is fine to
  // ignore.
  if (port_transferred_)
    return;

#if !defined(NDEBUG)
  ports::PortStatus port_status;
  node_controller_->node()->GetStatus(port_, &port_status);
  if (port_status.has_messages) {
    ports::ScopedMessage unused;
    size_t message_size = 0;
    node_controller_->node()->GetMessageIf(
        port_, [&message_size](const ports::Message& message) {
          message_size = message.num_payload_bytes();
          return false;
        }, &unused);
    DVLOG(1) << "New message detected on message pipe " << pipe_id_
             << " endpoint " << endpoint_ << " [port=" << port_.name()
             << "; size=" << message_size << "]";
  }
  if (port_status.peer_closed) {
    DVLOG(1) << "Peer closure detected on message pipe " << pipe_id_
             << " endpoint " << endpoint_ << " [port=" << port_.name() << "]";
  }
#endif

  awakables_.AwakeForStateChange(GetHandleSignalsStateNoLock());
}

}  // namespace edk
}  // namespace mojo
