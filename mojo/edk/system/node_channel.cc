// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/node_channel.h"

#include <cstring>
#include <limits>
#include <sstream>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/request_context.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "mojo/edk/system/mach_port_relay.h"
#endif

namespace mojo {
namespace edk {

namespace {

// NOTE: Please ONLY append messages to the end of this enum.
enum class MessageType : uint32_t {
  ACCEPT_CHILD,
  ACCEPT_PARENT,
  ADD_BROKER_CLIENT,
  BROKER_CLIENT_ADDED,
  ACCEPT_BROKER_CLIENT,
  EVENT_MESSAGE,
  REQUEST_PORT_MERGE,
  REQUEST_INTRODUCTION,
  INTRODUCE,
#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
  RELAY_EVENT_MESSAGE,
#endif
  BROADCAST_EVENT,
#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
  EVENT_MESSAGE_FROM_RELAY,
#endif
  ACCEPT_PEER,
};

struct Header {
  MessageType type;
  uint32_t padding;
};

static_assert(IsAlignedForChannelMessage(sizeof(Header)),
              "Invalid header size.");

struct AcceptChildData {
  ports::NodeName parent_name;
  ports::NodeName token;
};

struct AcceptParentData {
  ports::NodeName token;
  ports::NodeName child_name;
};

struct AcceptPeerData {
  ports::NodeName token;
  ports::NodeName peer_name;
  ports::PortName port_name;
};

// This message may include a process handle on plaforms that require it.
struct AddBrokerClientData {
  ports::NodeName client_name;
#if !defined(OS_WIN)
  uint32_t process_handle;
  uint32_t padding;
#endif
};

#if !defined(OS_WIN)
static_assert(sizeof(base::ProcessHandle) == sizeof(uint32_t),
              "Unexpected pid size");
static_assert(sizeof(AddBrokerClientData) % kChannelMessageAlignment == 0,
              "Invalid AddBrokerClientData size.");
#endif

// This data is followed by a platform channel handle to the broker.
struct BrokerClientAddedData {
  ports::NodeName client_name;
};

// This data may be followed by a platform channel handle to the broker. If not,
// then the parent is the broker and its channel should be used as such.
struct AcceptBrokerClientData {
  ports::NodeName broker_name;
};

// This is followed by arbitrary payload data which is interpreted as a token
// string for port location.
struct RequestPortMergeData {
  ports::PortName connector_port_name;
};

// Used for both REQUEST_INTRODUCTION and INTRODUCE.
//
// For INTRODUCE the message also includes a valid platform handle for a channel
// the receiver may use to communicate with the named node directly, or an
// invalid platform handle if the node is unknown to the sender or otherwise
// cannot be introduced.
struct IntroductionData {
  ports::NodeName name;
};

#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
// This struct is followed by the full payload of a message to be relayed.
struct RelayEventMessageData {
  ports::NodeName destination;
};

// This struct is followed by the full payload of a relayed message.
struct EventMessageFromRelayData {
  ports::NodeName source;
};
#endif

template <typename DataType>
Channel::MessagePtr CreateMessage(MessageType type,
                                  size_t payload_size,
                                  size_t num_handles,
                                  DataType** out_data,
                                  size_t capacity = 0) {
  const size_t total_size = payload_size + sizeof(Header);
  if (capacity == 0)
    capacity = total_size;
  else
    capacity = std::max(total_size, capacity);
  auto message =
      std::make_unique<Channel::Message>(capacity, total_size, num_handles);
  Header* header = reinterpret_cast<Header*>(message->mutable_payload());
  header->type = type;
  header->padding = 0;
  *out_data = reinterpret_cast<DataType*>(&header[1]);
  return message;
}

template <typename DataType>
bool GetMessagePayload(const void* bytes,
                       size_t num_bytes,
                       DataType** out_data) {
  static_assert(sizeof(DataType) > 0, "DataType must have non-zero size.");
  if (num_bytes < sizeof(Header) + sizeof(DataType))
    return false;
  *out_data = reinterpret_cast<const DataType*>(
      static_cast<const char*>(bytes) + sizeof(Header));
  return true;
}

}  // namespace

// static
scoped_refptr<NodeChannel> NodeChannel::Create(
    Delegate* delegate,
    ConnectionParams connection_params,
    scoped_refptr<base::TaskRunner> io_task_runner,
    const ProcessErrorCallback& process_error_callback) {
#if defined(OS_NACL_SFI)
  LOG(FATAL) << "Multi-process not yet supported on NaCl-SFI";
  return nullptr;
#else
  return new NodeChannel(delegate, std::move(connection_params), io_task_runner,
                         process_error_callback);
#endif
}

// static
Channel::MessagePtr NodeChannel::CreateEventMessage(size_t capacity,
                                                    size_t payload_size,
                                                    void** payload,
                                                    size_t num_handles) {
  return CreateMessage(MessageType::EVENT_MESSAGE, payload_size, num_handles,
                       payload, capacity);
}

// static
void NodeChannel::GetEventMessageData(Channel::Message* message,
                                      void** data,
                                      size_t* num_data_bytes) {
  // NOTE: OnChannelMessage guarantees that we never accept a Channel::Message
  // with a payload of fewer than |sizeof(Header)| bytes.
  *data = reinterpret_cast<Header*>(message->mutable_payload()) + 1;
  *num_data_bytes = message->payload_size() - sizeof(Header);
}

void NodeChannel::Start() {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  MachPortRelay* relay = delegate_->GetMachPortRelay();
  if (relay)
    relay->AddObserver(this);
#endif

  base::AutoLock lock(channel_lock_);
  // ShutDown() may have already been called, in which case |channel_| is null.
  if (channel_)
    channel_->Start();
}

void NodeChannel::ShutDown() {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  MachPortRelay* relay = delegate_->GetMachPortRelay();
  if (relay)
    relay->RemoveObserver(this);
#endif

  base::AutoLock lock(channel_lock_);
  if (channel_) {
    channel_->ShutDown();
    channel_ = nullptr;
  }
}

void NodeChannel::LeakHandleOnShutdown() {
  base::AutoLock lock(channel_lock_);
  if (channel_) {
    channel_->LeakHandle();
  }
}

void NodeChannel::NotifyBadMessage(const std::string& error) {
  if (!process_error_callback_.is_null())
    process_error_callback_.Run("Received bad user message: " + error);
}

void NodeChannel::SetRemoteProcessHandle(base::ProcessHandle process_handle) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  base::AutoLock lock(remote_process_handle_lock_);
  DCHECK_EQ(base::kNullProcessHandle, remote_process_handle_);
  CHECK_NE(remote_process_handle_, base::GetCurrentProcessHandle());
  remote_process_handle_ = process_handle;
#if defined(OS_WIN)
  DCHECK(!scoped_remote_process_handle_.is_valid());
  scoped_remote_process_handle_.reset(PlatformHandle(process_handle));
#endif
}

bool NodeChannel::HasRemoteProcessHandle() {
  base::AutoLock lock(remote_process_handle_lock_);
  return remote_process_handle_ != base::kNullProcessHandle;
}

base::ProcessHandle NodeChannel::CopyRemoteProcessHandle() {
  base::AutoLock lock(remote_process_handle_lock_);
#if defined(OS_WIN)
  if (remote_process_handle_ != base::kNullProcessHandle) {
    // Privileged nodes use this to pass their childrens' process handles to the
    // broker on launch.
    HANDLE handle = remote_process_handle_;
    BOOL result = DuplicateHandle(
        base::GetCurrentProcessHandle(), remote_process_handle_,
        base::GetCurrentProcessHandle(), &handle, 0, FALSE,
        DUPLICATE_SAME_ACCESS);
    DPCHECK(result);
    return handle;
  }
  return base::kNullProcessHandle;
#else
  return remote_process_handle_;
#endif
}

void NodeChannel::SetRemoteNodeName(const ports::NodeName& name) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  remote_node_name_ = name;
}

void NodeChannel::AcceptChild(const ports::NodeName& parent_name,
                              const ports::NodeName& token) {
  AcceptChildData* data;
  Channel::MessagePtr message = CreateMessage(
      MessageType::ACCEPT_CHILD, sizeof(AcceptChildData), 0, &data);
  data->parent_name = parent_name;
  data->token = token;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::AcceptParent(const ports::NodeName& token,
                               const ports::NodeName& child_name) {
  AcceptParentData* data;
  Channel::MessagePtr message = CreateMessage(
      MessageType::ACCEPT_PARENT, sizeof(AcceptParentData), 0, &data);
  data->token = token;
  data->child_name = child_name;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::AcceptPeer(const ports::NodeName& sender_name,
                             const ports::NodeName& token,
                             const ports::PortName& port_name) {
  AcceptPeerData* data;
  Channel::MessagePtr message =
      CreateMessage(MessageType::ACCEPT_PEER, sizeof(AcceptPeerData), 0, &data);
  data->token = token;
  data->peer_name = sender_name;
  data->port_name = port_name;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::AddBrokerClient(const ports::NodeName& client_name,
                                  base::ProcessHandle process_handle) {
  AddBrokerClientData* data;
  std::vector<ScopedPlatformHandle> handles;
#if defined(OS_WIN)
  handles.emplace_back(ScopedPlatformHandle(PlatformHandle(process_handle)));
#endif
  Channel::MessagePtr message =
      CreateMessage(MessageType::ADD_BROKER_CLIENT, sizeof(AddBrokerClientData),
                    handles.size(), &data);
  message->SetHandles(std::move(handles));
  data->client_name = client_name;
#if !defined(OS_WIN)
  data->process_handle = process_handle;
  data->padding = 0;
#endif
  WriteChannelMessage(std::move(message));
}

void NodeChannel::BrokerClientAdded(const ports::NodeName& client_name,
                                    ScopedPlatformHandle broker_channel) {
  BrokerClientAddedData* data;
  std::vector<ScopedPlatformHandle> handles;
  if (broker_channel.is_valid())
    handles.push_back(std::move(broker_channel));
  Channel::MessagePtr message =
      CreateMessage(MessageType::BROKER_CLIENT_ADDED,
                    sizeof(BrokerClientAddedData), handles.size(), &data);
  message->SetHandles(std::move(handles));
  data->client_name = client_name;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::AcceptBrokerClient(const ports::NodeName& broker_name,
                                     ScopedPlatformHandle broker_channel) {
  AcceptBrokerClientData* data;
  std::vector<ScopedPlatformHandle> handles;
  if (broker_channel.is_valid())
    handles.push_back(std::move(broker_channel));
  Channel::MessagePtr message =
      CreateMessage(MessageType::ACCEPT_BROKER_CLIENT,
                    sizeof(AcceptBrokerClientData), handles.size(), &data);
  message->SetHandles(std::move(handles));
  data->broker_name = broker_name;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::RequestPortMerge(const ports::PortName& connector_port_name,
                                   const std::string& token) {
  RequestPortMergeData* data;
  Channel::MessagePtr message = CreateMessage(
      MessageType::REQUEST_PORT_MERGE,
      sizeof(RequestPortMergeData) + token.size(), 0, &data);
  data->connector_port_name = connector_port_name;
  memcpy(data + 1, token.data(), token.size());
  WriteChannelMessage(std::move(message));
}

void NodeChannel::RequestIntroduction(const ports::NodeName& name) {
  IntroductionData* data;
  Channel::MessagePtr message = CreateMessage(
      MessageType::REQUEST_INTRODUCTION, sizeof(IntroductionData), 0, &data);
  data->name = name;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::Introduce(const ports::NodeName& name,
                            ScopedPlatformHandle channel_handle) {
  IntroductionData* data;
  std::vector<ScopedPlatformHandle> handles;
  if (channel_handle.is_valid())
    handles.push_back(std::move(channel_handle));
  Channel::MessagePtr message = CreateMessage(
      MessageType::INTRODUCE, sizeof(IntroductionData), handles.size(), &data);
  message->SetHandles(std::move(handles));
  data->name = name;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::SendChannelMessage(Channel::MessagePtr message) {
  WriteChannelMessage(std::move(message));
}

void NodeChannel::Broadcast(Channel::MessagePtr message) {
  DCHECK(!message->has_handles());
  void* data;
  Channel::MessagePtr broadcast_message = CreateMessage(
      MessageType::BROADCAST_EVENT, message->data_num_bytes(), 0, &data);
  memcpy(data, message->data(), message->data_num_bytes());
  WriteChannelMessage(std::move(broadcast_message));
}

#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
void NodeChannel::RelayEventMessage(const ports::NodeName& destination,
                                    Channel::MessagePtr message) {
#if defined(OS_WIN)
  DCHECK(message->has_handles());

  // Note that this is only used on Windows, and on Windows all platform
  // handles are included in the message data. We blindly copy all the data
  // here and the relay node (the parent) will duplicate handles as needed.
  size_t num_bytes = sizeof(RelayEventMessageData) + message->data_num_bytes();
  RelayEventMessageData* data;
  Channel::MessagePtr relay_message =
      CreateMessage(MessageType::RELAY_EVENT_MESSAGE, num_bytes, 0, &data);
  data->destination = destination;
  memcpy(data + 1, message->data(), message->data_num_bytes());

  // When the handles are duplicated in the parent, the source handles will
  // be closed. If the parent never receives this message then these handles
  // will leak, but that means something else has probably broken and the
  // sending process won't likely be around much longer.
  std::vector<ScopedPlatformHandle> handles = message->TakeHandles();
  for (auto& handle : handles)
    ignore_result(handle.release());

#else
  DCHECK(message->has_mach_ports());

  // On OSX, the handles are extracted from the relayed message and attached to
  // the wrapper. The broker then takes the handles attached to the wrapper and
  // moves them back to the relayed message. This is necessary because the
  // message may contain fds which need to be attached to the outer message so
  // that they can be transferred to the broker.
  std::vector<ScopedPlatformHandle> handles = message->TakeHandles();
  size_t num_bytes = sizeof(RelayEventMessageData) + message->data_num_bytes();
  RelayEventMessageData* data;
  Channel::MessagePtr relay_message = CreateMessage(
      MessageType::RELAY_EVENT_MESSAGE, num_bytes, handles.size(), &data);
  data->destination = destination;
  memcpy(data + 1, message->data(), message->data_num_bytes());
  relay_message->SetHandles(std::move(handles));
#endif  // defined(OS_WIN)

  WriteChannelMessage(std::move(relay_message));
}

void NodeChannel::EventMessageFromRelay(const ports::NodeName& source,
                                        Channel::MessagePtr message) {
  size_t num_bytes =
      sizeof(EventMessageFromRelayData) + message->payload_size();
  EventMessageFromRelayData* data;
  Channel::MessagePtr relayed_message =
      CreateMessage(MessageType::EVENT_MESSAGE_FROM_RELAY, num_bytes,
                    message->num_handles(), &data);
  data->source = source;
  if (message->payload_size())
    memcpy(data + 1, message->payload(), message->payload_size());
  relayed_message->SetHandles(message->TakeHandles());
  WriteChannelMessage(std::move(relayed_message));
}
#endif  // defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))

NodeChannel::NodeChannel(Delegate* delegate,
                         ConnectionParams connection_params,
                         scoped_refptr<base::TaskRunner> io_task_runner,
                         const ProcessErrorCallback& process_error_callback)
    : delegate_(delegate),
      io_task_runner_(io_task_runner),
      process_error_callback_(process_error_callback)
#if !defined(OS_NACL_SFI)
      ,
      channel_(
          Channel::Create(this, std::move(connection_params), io_task_runner_))
#endif
{
}

NodeChannel::~NodeChannel() {
  ShutDown();
}

void NodeChannel::OnChannelMessage(const void* payload,
                                   size_t payload_size,
                                   std::vector<ScopedPlatformHandle> handles) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  RequestContext request_context(RequestContext::Source::SYSTEM);

  // Ensure this NodeChannel stays alive through the extent of this method. The
  // delegate may have the only other reference to this object and it may choose
  // to drop it here in response to, e.g., a malformed message.
  scoped_refptr<NodeChannel> keepalive = this;

#if defined(OS_WIN)
  // If we receive handles from a known process, rewrite them to our own
  // process. This can occur when a privileged node receives handles directly
  // from a privileged descendant.
  {
    base::AutoLock lock(remote_process_handle_lock_);
    if (!handles.empty() &&
        remote_process_handle_ != base::kNullProcessHandle) {
      // Note that we explicitly mark the handles as being owned by the sending
      // process before rewriting them, in order to accommodate RewriteHandles'
      // internal sanity checks.
      for (auto& handle : handles)
        handle.get().owning_process = remote_process_handle_;
      if (!Channel::Message::RewriteHandles(remote_process_handle_,
                                            base::GetCurrentProcessHandle(),
                                            &handles)) {
        DLOG(ERROR) << "Received one or more invalid handles.";
      }
    } else if (!handles.empty()) {
      // Handles received by an unknown process must already be owned by us.
      for (auto& handle : handles)
        handle.get().owning_process = base::GetCurrentProcessHandle();
    }
  }
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  // If we're not the root, receive any mach ports from the message. If we're
  // the root, the only message containing mach ports should be a
  // RELAY_EVENT_MESSAGE.
  {
    MachPortRelay* relay = delegate_->GetMachPortRelay();
    if (!handles.empty() && !relay)
      MachPortRelay::ReceivePorts(&handles);
  }
#endif  // defined(OS_WIN)


  if (payload_size <= sizeof(Header)) {
    delegate_->OnChannelError(remote_node_name_, this);
    return;
  }

  const Header* header = static_cast<const Header*>(payload);
  switch (header->type) {
    case MessageType::ACCEPT_CHILD: {
      const AcceptChildData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        delegate_->OnAcceptChild(remote_node_name_, data->parent_name,
                                 data->token);
        return;
      }
      break;
    }

    case MessageType::ACCEPT_PARENT: {
      const AcceptParentData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        delegate_->OnAcceptParent(remote_node_name_, data->token,
                                  data->child_name);
        return;
      }
      break;
    }

    case MessageType::ADD_BROKER_CLIENT: {
      const AddBrokerClientData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
#if defined(OS_WIN)
        if (handles.size() != 1) {
          DLOG(ERROR) << "Dropping invalid AddBrokerClient message.";
          break;
        }
        delegate_->OnAddBrokerClient(remote_node_name_, data->client_name,
                                     handles.at(0).release().handle);
#else
        if (!handles.empty()) {
          DLOG(ERROR) << "Dropping invalid AddBrokerClient message.";
          break;
        }
        delegate_->OnAddBrokerClient(remote_node_name_, data->client_name,
                                     data->process_handle);
#endif
        return;
      }
      break;
    }

    case MessageType::BROKER_CLIENT_ADDED: {
      const BrokerClientAddedData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        if (handles.size() != 1) {
          DLOG(ERROR) << "Dropping invalid BrokerClientAdded message.";
          break;
        }
        delegate_->OnBrokerClientAdded(remote_node_name_, data->client_name,
                                       std::move(handles.at(0)));
        return;
      }
      break;
    }

    case MessageType::ACCEPT_BROKER_CLIENT: {
      const AcceptBrokerClientData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        ScopedPlatformHandle broker_channel;
        if (handles.size() > 1) {
          DLOG(ERROR) << "Dropping invalid AcceptBrokerClient message.";
          break;
        }
        if (handles.size() == 1) {
          broker_channel = std::move(handles.at(0));
        }
        delegate_->OnAcceptBrokerClient(remote_node_name_, data->broker_name,
                                        std::move(broker_channel));
        return;
      }
      break;
    }

    case MessageType::EVENT_MESSAGE: {
      Channel::MessagePtr message(
          new Channel::Message(payload_size, handles.size()));
      message->SetHandles(std::move(handles));
      memcpy(message->mutable_payload(), payload, payload_size);
      delegate_->OnEventMessage(remote_node_name_, std::move(message));
      return;
    }

    case MessageType::REQUEST_PORT_MERGE: {
      const RequestPortMergeData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        // Don't accept an empty token.
        size_t token_size = payload_size - sizeof(*data) - sizeof(Header);
        if (token_size == 0)
          break;
        std::string token(reinterpret_cast<const char*>(data + 1), token_size);
        delegate_->OnRequestPortMerge(remote_node_name_,
                                      data->connector_port_name, token);
        return;
      }
      break;
    }

    case MessageType::REQUEST_INTRODUCTION: {
      const IntroductionData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        delegate_->OnRequestIntroduction(remote_node_name_, data->name);
        return;
      }
      break;
    }

    case MessageType::INTRODUCE: {
      const IntroductionData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        if (handles.size() > 1) {
          DLOG(ERROR) << "Dropping invalid introduction message.";
          break;
        }
        ScopedPlatformHandle channel_handle;
        if (handles.size() == 1) {
          channel_handle = std::move(handles.at(0));
        }
        delegate_->OnIntroduce(remote_node_name_, data->name,
                               std::move(channel_handle));
        return;
      }
      break;
    }

#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
    case MessageType::RELAY_EVENT_MESSAGE: {
      base::ProcessHandle from_process;
      {
        base::AutoLock lock(remote_process_handle_lock_);
        from_process = remote_process_handle_;
      }
      const RelayEventMessageData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        // Don't try to relay an empty message.
        if (payload_size <= sizeof(Header) + sizeof(RelayEventMessageData))
          break;

        const void* message_start = data + 1;
        Channel::MessagePtr message = Channel::Message::Deserialize(
            message_start, payload_size - sizeof(Header) - sizeof(*data));
        if (!message) {
          DLOG(ERROR) << "Dropping invalid relay message.";
          break;
        }
  #if defined(OS_MACOSX) && !defined(OS_IOS)
        message->SetHandles(std::move(handles));
        MachPortRelay* relay = delegate_->GetMachPortRelay();
        if (!relay) {
          LOG(ERROR) << "Receiving mach ports without a port relay from "
                     << remote_node_name_ << ". Dropping message.";
          break;
        }
        {
          base::AutoLock lock(pending_mach_messages_lock_);
          if (relay->port_provider()->TaskForPid(from_process) ==
              MACH_PORT_NULL) {
            pending_relay_messages_.push(
                std::make_pair(data->destination, std::move(message)));
            return;
          }
        }
  #endif
        delegate_->OnRelayEventMessage(remote_node_name_, from_process,
                                       data->destination, std::move(message));
        return;
      }
      break;
    }
#endif

    case MessageType::BROADCAST_EVENT: {
      if (payload_size <= sizeof(Header))
        break;
      const void* data = static_cast<const void*>(
          reinterpret_cast<const Header*>(payload) + 1);
      Channel::MessagePtr message =
          Channel::Message::Deserialize(data, payload_size - sizeof(Header));
      if (!message || message->has_handles()) {
        DLOG(ERROR) << "Dropping invalid broadcast message.";
        break;
      }
      delegate_->OnBroadcast(remote_node_name_, std::move(message));
      return;
    }

#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
    case MessageType::EVENT_MESSAGE_FROM_RELAY:
      const EventMessageFromRelayData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        size_t num_bytes = payload_size - sizeof(*data);
        if (num_bytes < sizeof(Header))
          break;
        num_bytes -= sizeof(Header);

        Channel::MessagePtr message(
            new Channel::Message(num_bytes, handles.size()));
        message->SetHandles(std::move(handles));
        if (num_bytes)
          memcpy(message->mutable_payload(), data + 1, num_bytes);
        delegate_->OnEventMessageFromRelay(remote_node_name_, data->source,
                                           std::move(message));
        return;
      }
      break;

#endif  // defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))

    case MessageType::ACCEPT_PEER: {
      const AcceptPeerData* data;
      if (GetMessagePayload(payload, payload_size, &data)) {
        delegate_->OnAcceptPeer(remote_node_name_, data->token, data->peer_name,
                                data->port_name);
        return;
      }
      break;
    }

    default:
      // Ignore unrecognized message types, allowing for future extensibility.
      return;
  }

  DLOG(ERROR) << "Received invalid message. Closing channel.";
  if (process_error_callback_)
    process_error_callback_.Run("NodeChannel received a malformed message");
  delegate_->OnChannelError(remote_node_name_, this);
}

void NodeChannel::OnChannelError(Channel::Error error) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  RequestContext request_context(RequestContext::Source::SYSTEM);

  ShutDown();

  if (process_error_callback_ &&
      error == Channel::Error::kReceivedMalformedData) {
    process_error_callback_.Run("Channel received a malformed message");
  }

  // |OnChannelError()| may cause |this| to be destroyed, but still need access
  // to the name name after that destruction. So may a copy of
  // |remote_node_name_| so it can be used if |this| becomes destroyed.
  ports::NodeName node_name = remote_node_name_;
  delegate_->OnChannelError(node_name, this);
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
void NodeChannel::OnProcessReady(base::ProcessHandle process) {
  io_task_runner_->PostTask(FROM_HERE, base::Bind(
      &NodeChannel::ProcessPendingMessagesWithMachPorts, this));
}

void NodeChannel::ProcessPendingMessagesWithMachPorts() {
  MachPortRelay* relay = delegate_->GetMachPortRelay();
  DCHECK(relay);

  base::ProcessHandle remote_process_handle;
  {
    base::AutoLock lock(remote_process_handle_lock_);
    remote_process_handle = remote_process_handle_;
  }
  PendingMessageQueue pending_writes;
  PendingRelayMessageQueue pending_relays;
  {
    base::AutoLock lock(pending_mach_messages_lock_);
    pending_writes.swap(pending_write_messages_);
    pending_relays.swap(pending_relay_messages_);
  }

  while (!pending_writes.empty()) {
    Channel::MessagePtr message = std::move(pending_writes.front());
    pending_writes.pop();
    relay->SendPortsToProcess(message.get(), remote_process_handle);

    base::AutoLock lock(channel_lock_);
    if (!channel_) {
      DLOG(ERROR) << "Dropping message on closed channel.";
      break;
    } else {
      channel_->Write(std::move(message));
    }
  }

  // Ensure this NodeChannel stays alive while flushing relay messages.
  scoped_refptr<NodeChannel> keepalive = this;

  while (!pending_relays.empty()) {
    ports::NodeName destination = pending_relays.front().first;
    Channel::MessagePtr message = std::move(pending_relays.front().second);
    pending_relays.pop();
    delegate_->OnRelayEventMessage(remote_node_name_, remote_process_handle,
                                   destination, std::move(message));
  }
}
#endif

void NodeChannel::WriteChannelMessage(Channel::MessagePtr message) {
#if defined(OS_WIN)
  // Map handles to the destination process. Note: only messages from a
  // privileged node should contain handles on Windows. If an unprivileged
  // node needs to send handles, it should do so via RelayEventMessage which
  // stashes the handles in the message in such a way that they go undetected
  // here (they'll be unpacked and duplicated by a privileged parent.)

  if (message->has_handles()) {
    base::ProcessHandle remote_process_handle;
    {
      base::AutoLock lock(remote_process_handle_lock_);
      remote_process_handle = remote_process_handle_;
    }

    // Rewrite outgoing handles if we have a handle to the destination process.
    if (remote_process_handle != base::kNullProcessHandle) {
      std::vector<ScopedPlatformHandle> handles = message->TakeHandles();
      if (!Channel::Message::RewriteHandles(base::GetCurrentProcessHandle(),
                                            remote_process_handle, &handles)) {
        DLOG(ERROR) << "Failed to duplicate one or more outgoing handles.";
      }
      message->SetHandles(std::move(handles));
    }
  }
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  // On OSX, we need to transfer mach ports to the destination process before
  // transferring the message itself.
  if (message->has_mach_ports()) {
    MachPortRelay* relay = delegate_->GetMachPortRelay();
    if (relay) {
      base::ProcessHandle remote_process_handle;
      {
        base::AutoLock lock(remote_process_handle_lock_);
        // Expect that the receiving node is a child.
        DCHECK(remote_process_handle_ != base::kNullProcessHandle);
        remote_process_handle = remote_process_handle_;
      }
      {
        base::AutoLock lock(pending_mach_messages_lock_);
        if (relay->port_provider()->TaskForPid(remote_process_handle) ==
            MACH_PORT_NULL) {
          // It is also possible for TaskForPid() to return MACH_PORT_NULL when
          // the process has started, then died. In that case, the queued
          // message will never be processed. But that's fine since we're about
          // to die anyway.
          pending_write_messages_.push(std::move(message));
          return;
        }
      }

      relay->SendPortsToProcess(message.get(), remote_process_handle);
    }
  }
#endif

  // Force a crash if this process attempts to send a message larger than the
  // maximum allowed size. This is more useful than killing a Channel when we
  // *receive* an oversized message, as we should consider oversized message
  // transmission to be a bug and this helps easily identify offending code.
  CHECK(message->data_num_bytes() < GetConfiguration().max_message_num_bytes);

  base::AutoLock lock(channel_lock_);
  if (!channel_)
    DLOG(ERROR) << "Dropping message on closed channel.";
  else
    channel_->Write(std::move(message));
}

}  // namespace edk
}  // namespace mojo
