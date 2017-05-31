// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/message_pipe_dispatcher.h"

#include <limits>
#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/message_for_transit.h"
#include "mojo/edk/system/node_controller.h"
#include "mojo/edk/system/ports/message_filter.h"
#include "mojo/edk/system/ports_message.h"
#include "mojo/edk/system/request_context.h"

namespace mojo {
namespace edk {

namespace {

using DispatcherHeader = MessageForTransit::DispatcherHeader;
using MessageHeader = MessageForTransit::MessageHeader;

#pragma pack(push, 1)

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

// A MessageFilter used by ReadMessage to determine whether a message should
// actually be consumed yet.
class ReadMessageFilter : public ports::MessageFilter {
 public:
  // Creates a new ReadMessageFilter which captures and potentially modifies
  // various (unowned) local state within MessagePipeDispatcher::ReadMessage.
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
  bool Match(const ports::Message& m) override {
    const PortsMessage& message = static_cast<const PortsMessage&>(m);
    if (message.num_payload_bytes() < sizeof(MessageHeader)) {
      *invalid_message_ = true;
      return true;
    }

    const MessageHeader* header =
        static_cast<const MessageHeader*>(message.payload_bytes());
    if (header->header_size > message.num_payload_bytes()) {
      *invalid_message_ = true;
      return true;
    }

    uint32_t bytes_to_read = 0;
    uint32_t bytes_available =
        static_cast<uint32_t>(message.num_payload_bytes()) -
        header->header_size;
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

#if DCHECK_IS_ON()

// A MessageFilter which never matches a message. Used to peek at the size of
// the next available message on a port, for debug logging only.
class PeekSizeMessageFilter : public ports::MessageFilter {
 public:
  PeekSizeMessageFilter() {}
  ~PeekSizeMessageFilter() override {}

  // ports::MessageFilter:
  bool Match(const ports::Message& message) override {
    message_size_ = message.num_payload_bytes();
    return false;
  }

  size_t message_size() const { return message_size_; }

 private:
  size_t message_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PeekSizeMessageFilter);
};

#endif  // DCHECK_IS_ON()

MessagePipeDispatcher::MessagePipeDispatcher(NodeController* node_controller,
                                             const ports::PortRef& port,
                                             uint64_t pipe_id,
                                             int endpoint)
    : node_controller_(node_controller),
      port_(port),
      pipe_id_(pipe_id),
      endpoint_(endpoint),
      watchers_(this) {
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
    watchers_.NotifyClosed();
  }

  ports::PortRef port1;
  {
    base::AutoLock lock(other->signal_lock_);
    port1 = other->port_;
    other->port_closed_.Set(true);
    other->watchers_.NotifyClosed();
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
  DVLOG(2) << "Closing message pipe " << pipe_id_ << " endpoint " << endpoint_
           << " [port=" << port_.name() << "]";
  return CloseNoLock();
}

MojoResult MessagePipeDispatcher::WriteMessage(
    std::unique_ptr<MessageForTransit> message,
    MojoWriteMessageFlags flags) {
  if (port_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  size_t num_bytes = message->num_bytes();
  int rv = node_controller_->SendMessage(port_, message->TakePortsMessage());

  DVLOG(4) << "Sent message on pipe " << pipe_id_ << " endpoint " << endpoint_
           << " [port=" << port_.name() << "; rv=" << rv
           << "; num_bytes=" << num_bytes << "]";

  if (rv != ports::OK) {
    if (rv == ports::ERROR_PORT_UNKNOWN ||
        rv == ports::ERROR_PORT_STATE_UNEXPECTED ||
        rv == ports::ERROR_PORT_CANNOT_SEND_PEER) {
      return MOJO_RESULT_INVALID_ARGUMENT;
    } else if (rv == ports::ERROR_PORT_PEER_CLOSED) {
      return MOJO_RESULT_FAILED_PRECONDITION;
    }

    NOTREACHED();
    return MOJO_RESULT_UNKNOWN;
  }

  return MOJO_RESULT_OK;
}

MojoResult MessagePipeDispatcher::ReadMessage(
    std::unique_ptr<MessageForTransit>* message,
    uint32_t* num_bytes,
    MojoHandle* handles,
    uint32_t* num_handles,
    MojoReadMessageFlags flags,
    bool read_any_size) {
  // We can't read from a port that's closed or in transit!
  if (port_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  bool no_space = false;
  bool may_discard = flags & MOJO_READ_MESSAGE_FLAG_MAY_DISCARD;
  bool invalid_message = false;

  // Grab a message if the provided handles buffer is large enough. If the input
  // |num_bytes| is provided and |read_any_size| is false, we also ensure
  // that it specifies a size at least as large as the next available payload.
  //
  // If |read_any_size| is true, the input value of |*num_bytes| is ignored.
  // This flag exists to support both new and old API behavior.

  ports::ScopedMessage ports_message;
  ReadMessageFilter filter(read_any_size, may_discard, num_bytes, num_handles,
                           &no_space, &invalid_message);
  int rv = node_controller_->node()->GetMessage(port_, &ports_message, &filter);

  if (invalid_message)
    return MOJO_RESULT_UNKNOWN;

  if (rv != ports::OK && rv != ports::ERROR_PORT_PEER_CLOSED) {
    if (rv == ports::ERROR_PORT_UNKNOWN ||
        rv == ports::ERROR_PORT_STATE_UNEXPECTED)
      return MOJO_RESULT_INVALID_ARGUMENT;

    NOTREACHED();
    return MOJO_RESULT_UNKNOWN;  // TODO: Add a better error code here?
  }

  if (no_space) {
    if (may_discard) {
      // May have been the last message on the pipe. Need to update signals just
      // in case.
      base::AutoLock lock(signal_lock_);
      watchers_.NotifyState(GetHandleSignalsStateNoLock());
    }
    // |*num_handles| (and/or |*num_bytes| if |read_any_size| is false) wasn't
    // sufficient to hold this message's data. The message will still be in
    // queue unless MOJO_READ_MESSAGE_FLAG_MAY_DISCARD was set.
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  if (!ports_message) {
    // No message was available in queue.

    if (rv == ports::OK)
      return MOJO_RESULT_SHOULD_WAIT;

    // Peer is closed and there are no more messages to read.
    DCHECK_EQ(rv, ports::ERROR_PORT_PEER_CLOSED);
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  // Alright! We have a message and the caller has provided sufficient storage
  // in which to receive it.

  {
    // We need to update anyone watching our signals in case that was the last
    // available message.
    base::AutoLock lock(signal_lock_);
    watchers_.NotifyState(GetHandleSignalsStateNoLock());
  }

  std::unique_ptr<PortsMessage> msg(
      static_cast<PortsMessage*>(ports_message.release()));

  const MessageHeader* header =
      static_cast<const MessageHeader*>(msg->payload_bytes());
  const DispatcherHeader* dispatcher_headers =
      reinterpret_cast<const DispatcherHeader*>(header + 1);

  if (header->num_dispatchers > std::numeric_limits<uint16_t>::max())
    return MOJO_RESULT_UNKNOWN;

  // Deserialize dispatchers.
  if (header->num_dispatchers > 0) {
    CHECK(handles);
    std::vector<DispatcherInTransit> dispatchers(header->num_dispatchers);
    size_t data_payload_index = sizeof(MessageHeader) +
        header->num_dispatchers * sizeof(DispatcherHeader);
    if (data_payload_index > header->header_size)
      return MOJO_RESULT_UNKNOWN;
    const char* dispatcher_data = reinterpret_cast<const char*>(
        dispatcher_headers + header->num_dispatchers);
    size_t port_index = 0;
    size_t platform_handle_index = 0;
    ScopedPlatformHandleVectorPtr msg_handles = msg->TakeHandles();
    const size_t num_msg_handles = msg_handles ? msg_handles->size() : 0;
    for (size_t i = 0; i < header->num_dispatchers; ++i) {
      const DispatcherHeader& dh = dispatcher_headers[i];
      Type type = static_cast<Type>(dh.type);

      size_t next_payload_index = data_payload_index + dh.num_bytes;
      if (msg->num_payload_bytes() < next_payload_index ||
          next_payload_index < data_payload_index) {
        return MOJO_RESULT_UNKNOWN;
      }

      size_t next_port_index = port_index + dh.num_ports;
      if (msg->num_ports() < next_port_index || next_port_index < port_index)
        return MOJO_RESULT_UNKNOWN;

      size_t next_platform_handle_index =
          platform_handle_index + dh.num_platform_handles;
      if (num_msg_handles < next_platform_handle_index ||
          next_platform_handle_index < platform_handle_index) {
        return MOJO_RESULT_UNKNOWN;
      }

      PlatformHandle* out_handles =
          num_msg_handles ? msg_handles->data() + platform_handle_index
                          : nullptr;
      dispatchers[i].dispatcher = Dispatcher::Deserialize(
          type, dispatcher_data, dh.num_bytes, msg->ports() + port_index,
          dh.num_ports, out_handles, dh.num_platform_handles);
      if (!dispatchers[i].dispatcher)
        return MOJO_RESULT_UNKNOWN;

      dispatcher_data += dh.num_bytes;
      data_payload_index = next_payload_index;
      port_index = next_port_index;
      platform_handle_index = next_platform_handle_index;
    }

    if (!node_controller_->core()->AddDispatchersFromTransit(dispatchers,
                                                             handles))
      return MOJO_RESULT_UNKNOWN;
  }

  CHECK(msg);
  *message = MessageForTransit::WrapPortsMessage(std::move(msg));
  return MOJO_RESULT_OK;
}

HandleSignalsState
MessagePipeDispatcher::GetHandleSignalsState() const {
  base::AutoLock lock(signal_lock_);
  return GetHandleSignalsStateNoLock();
}

MojoResult MessagePipeDispatcher::AddWatcherRef(
    const scoped_refptr<WatcherDispatcher>& watcher,
    uintptr_t context) {
  base::AutoLock lock(signal_lock_);
  if (port_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;
  return watchers_.Add(watcher, context, GetHandleSignalsStateNoLock());
}

MojoResult MessagePipeDispatcher::RemoveWatcherRef(WatcherDispatcher* watcher,
                                                   uintptr_t context) {
  base::AutoLock lock(signal_lock_);
  if (port_closed_ || in_transit_)
    return MOJO_RESULT_INVALID_ARGUMENT;
  return watchers_.Remove(watcher, context);
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
  watchers_.NotifyState(GetHandleSignalsStateNoLock());
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
  watchers_.NotifyClosed();

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

#if DCHECK_IS_ON()
  ports::PortStatus port_status;
  if (node_controller_->node()->GetStatus(port_, &port_status) == ports::OK) {
    if (port_status.has_messages) {
      ports::ScopedMessage unused;
      PeekSizeMessageFilter filter;
      node_controller_->node()->GetMessage(port_, &unused, &filter);
      DVLOG(4) << "New message detected on message pipe " << pipe_id_
               << " endpoint " << endpoint_ << " [port=" << port_.name()
               << "; size=" << filter.message_size() << "]";
    }
    if (port_status.peer_closed) {
      DVLOG(2) << "Peer closure detected on message pipe " << pipe_id_
               << " endpoint " << endpoint_ << " [port=" << port_.name() << "]";
    }
  }
#endif

  watchers_.NotifyState(GetHandleSignalsStateNoLock());
}

}  // namespace edk
}  // namespace mojo
