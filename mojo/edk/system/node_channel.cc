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
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/request_context.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "mojo/edk/system/mach_port_relay.h"
#endif

namespace mojo {
namespace edk {

namespace {

template <typename T>
T Align(T t) {
  const auto k = kChannelMessageAlignment;
  return t + (k - (t % k)) % k;
}

enum class MessageType : uint32_t {
  ACCEPT_CHILD,
  ACCEPT_PARENT,
  ADD_BROKER_CLIENT,
  BROKER_CLIENT_ADDED,
  ACCEPT_BROKER_CLIENT,
  PORTS_MESSAGE,
  REQUEST_PORT_MERGE,
  REQUEST_INTRODUCTION,
  INTRODUCE,
#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
  RELAY_PORTS_MESSAGE,
#endif
};

struct Header {
  MessageType type;
  uint32_t padding;
};

static_assert(sizeof(Header) % kChannelMessageAlignment == 0,
    "Invalid header size.");

struct AcceptChildData {
  ports::NodeName parent_name;
  ports::NodeName token;
};

struct AcceptParentData {
  ports::NodeName token;
  ports::NodeName child_name;
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
struct RelayPortsMessageData {
  ports::NodeName destination;
};
#endif

template <typename DataType>
Channel::MessagePtr CreateMessage(MessageType type,
                                  size_t payload_size,
                                  size_t num_handles,
                                  DataType** out_data) {
  Channel::MessagePtr message(
      new Channel::Message(sizeof(Header) + payload_size, num_handles));
  Header* header = reinterpret_cast<Header*>(message->mutable_payload());
  header->type = type;
  header->padding = 0;
  *out_data = reinterpret_cast<DataType*>(&header[1]);
  return message;
}

template <typename DataType>
void GetMessagePayload(const void* bytes, DataType** out_data) {
  *out_data = reinterpret_cast<const DataType*>(
      static_cast<const char*>(bytes) + sizeof(Header));
}

}  // namespace

// static
scoped_refptr<NodeChannel> NodeChannel::Create(
    Delegate* delegate,
    ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_task_runner) {
  return new NodeChannel(delegate, std::move(platform_handle), io_task_runner);
}

// static
Channel::MessagePtr NodeChannel::CreatePortsMessage(size_t payload_size,
                                                    void** payload,
                                                    size_t num_handles) {
  return CreateMessage(MessageType::PORTS_MESSAGE, payload_size, num_handles,
                       payload);
}

// static
void NodeChannel::GetPortsMessageData(Channel::Message* message,
                                      void** data,
                                      size_t* num_data_bytes) {
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
  DCHECK(channel_);
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

void NodeChannel::SetRemoteProcessHandle(base::ProcessHandle process_handle) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  base::AutoLock lock(remote_process_handle_lock_);
  CHECK_NE(remote_process_handle_, base::GetCurrentProcessHandle());
  remote_process_handle_ = process_handle;
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
    DCHECK(result);
    return handle;
  }
  return base::kNullProcessHandle;
#else
  return remote_process_handle_;
#endif
}

void NodeChannel::SetRemoteNodeName(const ports::NodeName& name) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
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

void NodeChannel::AddBrokerClient(const ports::NodeName& client_name,
                                  base::ProcessHandle process_handle) {
  AddBrokerClientData* data;
  ScopedPlatformHandleVectorPtr handles(new PlatformHandleVector());
#if defined(OS_WIN)
  handles->push_back(PlatformHandle(process_handle));
#endif
  Channel::MessagePtr message = CreateMessage(
      MessageType::ADD_BROKER_CLIENT, sizeof(AddBrokerClientData),
      handles->size(), &data);
  message->SetHandles(std::move(handles));
  data->client_name = client_name;
#if !defined(OS_WIN)
  data->process_handle = process_handle;
#endif
  WriteChannelMessage(std::move(message));
}

void NodeChannel::BrokerClientAdded(const ports::NodeName& client_name,
                                    ScopedPlatformHandle broker_channel) {
  BrokerClientAddedData* data;
  ScopedPlatformHandleVectorPtr handles(new PlatformHandleVector());
  if (broker_channel.is_valid())
    handles->push_back(broker_channel.release());
  Channel::MessagePtr message = CreateMessage(
      MessageType::BROKER_CLIENT_ADDED, sizeof(BrokerClientAddedData),
      handles->size(), &data);
  message->SetHandles(std::move(handles));
  data->client_name = client_name;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::AcceptBrokerClient(const ports::NodeName& broker_name,
                                     ScopedPlatformHandle broker_channel) {
  AcceptBrokerClientData* data;
  ScopedPlatformHandleVectorPtr handles(new PlatformHandleVector());
  if (broker_channel.is_valid())
    handles->push_back(broker_channel.release());
  Channel::MessagePtr message = CreateMessage(
      MessageType::ACCEPT_BROKER_CLIENT, sizeof(AcceptBrokerClientData),
      handles->size(), &data);
  message->SetHandles(std::move(handles));
  data->broker_name = broker_name;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::PortsMessage(Channel::MessagePtr message) {
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
  ScopedPlatformHandleVectorPtr handles(new PlatformHandleVector());
  if (channel_handle.is_valid())
    handles->push_back(channel_handle.release());
  Channel::MessagePtr message = CreateMessage(
      MessageType::INTRODUCE, sizeof(IntroductionData), handles->size(), &data);
  message->SetHandles(std::move(handles));
  data->name = name;
  WriteChannelMessage(std::move(message));
}

#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
void NodeChannel::RelayPortsMessage(const ports::NodeName& destination,
                                    Channel::MessagePtr message) {
#if defined(OS_WIN)
  DCHECK(message->has_handles());

  // Note that this is only used on Windows, and on Windows all platform
  // handles are included in the message data. We blindly copy all the data
  // here and the relay node (the parent) will duplicate handles as needed.
  size_t num_bytes = sizeof(RelayPortsMessageData) + message->data_num_bytes();
  RelayPortsMessageData* data;
  Channel::MessagePtr relay_message = CreateMessage(
      MessageType::RELAY_PORTS_MESSAGE, num_bytes, 0, &data);
  data->destination = destination;
  memcpy(data + 1, message->data(), message->data_num_bytes());

  // When the handles are duplicated in the parent, the source handles will
  // be closed. If the parent never receives this message then these handles
  // will leak, but that means something else has probably broken and the
  // sending process won't likely be around much longer.
  ScopedPlatformHandleVectorPtr handles = message->TakeHandles();
  handles->clear();

#else
  DCHECK(message->has_mach_ports());

  // On OSX, the handles are extracted from the relayed message and attached to
  // the wrapper. The broker then takes the handles attached to the wrapper and
  // moves them back to the relayed message. This is necessary because the
  // message may contain fds which need to be attached to the outer message so
  // that they can be transferred to the broker.
  ScopedPlatformHandleVectorPtr handles = message->TakeHandles();
  size_t num_bytes = sizeof(RelayPortsMessageData) + message->data_num_bytes();
  RelayPortsMessageData* data;
  Channel::MessagePtr relay_message = CreateMessage(
      MessageType::RELAY_PORTS_MESSAGE, num_bytes, handles->size(), &data);
  data->destination = destination;
  memcpy(data + 1, message->data(), message->data_num_bytes());
  relay_message->SetHandles(std::move(handles));
#endif  // defined(OS_WIN)

  WriteChannelMessage(std::move(relay_message));
}
#endif  // defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))

NodeChannel::NodeChannel(Delegate* delegate,
                         ScopedPlatformHandle platform_handle,
                         scoped_refptr<base::TaskRunner> io_task_runner)
    : delegate_(delegate),
      io_task_runner_(io_task_runner),
      channel_(
          Channel::Create(this, std::move(platform_handle), io_task_runner_)) {
}

NodeChannel::~NodeChannel() {
  ShutDown();
}

void NodeChannel::OnChannelMessage(const void* payload,
                                   size_t payload_size,
                                   ScopedPlatformHandleVectorPtr handles) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  RequestContext request_context(RequestContext::Source::SYSTEM);

#if defined(OS_WIN)
  // If we receive handles from a known process, rewrite them to our own
  // process. This can occur when a privileged node receives handles directly
  // from a privileged descendant.
  {
    base::AutoLock lock(remote_process_handle_lock_);
    if (handles && remote_process_handle_ != base::kNullProcessHandle) {
      // Note that we explicitly mark the handles as being owned by the sending
      // process before rewriting them, in order to accommodate RewriteHandles'
      // internal sanity checks.
      for (auto& handle : *handles)
        handle.owning_process = remote_process_handle_;
      if (!Channel::Message::RewriteHandles(remote_process_handle_,
                                            base::GetCurrentProcessHandle(),
                                            handles->data(), handles->size())) {
        DLOG(ERROR) << "Received one or more invalid handles.";
      }
    } else if (handles) {
      // Handles received by an unknown process must already be owned by us.
      for (auto& handle : *handles)
        handle.owning_process = base::GetCurrentProcessHandle();
    }
  }
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  // If we're not the root, receive any mach ports from the message. If we're
  // the root, the only message containing mach ports should be a
  // RELAY_PORTS_MESSAGE.
  {
    MachPortRelay* relay = delegate_->GetMachPortRelay();
    if (handles && !relay) {
      if (!MachPortRelay::ReceivePorts(handles.get())) {
        LOG(ERROR) << "Error receiving mach ports.";
      }
    }
  }
#endif  // defined(OS_WIN)

  const Header* header = static_cast<const Header*>(payload);
  switch (header->type) {
    case MessageType::ACCEPT_CHILD: {
      const AcceptChildData* data;
      GetMessagePayload(payload, &data);
      delegate_->OnAcceptChild(remote_node_name_, data->parent_name,
                               data->token);
      break;
    }

    case MessageType::ACCEPT_PARENT: {
      const AcceptParentData* data;
      GetMessagePayload(payload, &data);
      delegate_->OnAcceptParent(remote_node_name_, data->token,
                                data->child_name);
      break;
    }

    case MessageType::ADD_BROKER_CLIENT: {
      const AddBrokerClientData* data;
      GetMessagePayload(payload, &data);
      ScopedPlatformHandle process_handle;
#if defined(OS_WIN)
      if (!handles || handles->size() != 1) {
        DLOG(ERROR) << "Dropping invalid AddBrokerClient message.";
        break;
      }
      process_handle = ScopedPlatformHandle(handles->at(0));
      handles->clear();
      delegate_->OnAddBrokerClient(remote_node_name_, data->client_name,
                                   process_handle.release().handle);
#else
      if (handles && handles->size() != 0) {
        DLOG(ERROR) << "Dropping invalid AddBrokerClient message.";
        break;
      }
      delegate_->OnAddBrokerClient(remote_node_name_, data->client_name,
                                   data->process_handle);
#endif
      break;
    }

    case MessageType::BROKER_CLIENT_ADDED: {
      const BrokerClientAddedData* data;
      GetMessagePayload(payload, &data);
      ScopedPlatformHandle broker_channel;
      if (!handles || handles->size() != 1) {
        DLOG(ERROR) << "Dropping invalid BrokerClientAdded message.";
        break;
      }
      broker_channel = ScopedPlatformHandle(handles->at(0));
      handles->clear();
      delegate_->OnBrokerClientAdded(remote_node_name_, data->client_name,
                                     std::move(broker_channel));
      break;
    }

    case MessageType::ACCEPT_BROKER_CLIENT: {
      const AcceptBrokerClientData* data;
      GetMessagePayload(payload, &data);
      ScopedPlatformHandle broker_channel;
      if (handles && handles->size() > 1) {
        DLOG(ERROR) << "Dropping invalid AcceptBrokerClient message.";
        break;
      }
      if (handles && handles->size() == 1) {
        broker_channel = ScopedPlatformHandle(handles->at(0));
        handles->clear();
      }
      delegate_->OnAcceptBrokerClient(remote_node_name_, data->broker_name,
                                      std::move(broker_channel));
      break;
    }

    case MessageType::PORTS_MESSAGE: {
      size_t num_handles = handles ? handles->size() : 0;
      Channel::MessagePtr message(
          new Channel::Message(payload_size, num_handles));
      message->SetHandles(std::move(handles));
      memcpy(message->mutable_payload(), payload, payload_size);
      delegate_->OnPortsMessage(std::move(message));
      break;
    }

    case MessageType::REQUEST_PORT_MERGE: {
      const RequestPortMergeData* data;
      GetMessagePayload(payload, &data);

      const char* token_data = reinterpret_cast<const char*>(data + 1);
      const size_t token_size = payload_size - sizeof(*data) - sizeof(Header);
      std::string token(token_data, token_size);

      delegate_->OnRequestPortMerge(remote_node_name_,
                                    data->connector_port_name, token);
      break;
    }

    case MessageType::REQUEST_INTRODUCTION: {
      const IntroductionData* data;
      GetMessagePayload(payload, &data);
      delegate_->OnRequestIntroduction(remote_node_name_, data->name);
      break;
    }

    case MessageType::INTRODUCE: {
      const IntroductionData* data;
      GetMessagePayload(payload, &data);
      if (handles && handles->size() > 1) {
        DLOG(ERROR) << "Dropping invalid introduction message.";
        break;
      }
      ScopedPlatformHandle channel_handle;
      if (handles && handles->size() == 1) {
        channel_handle = ScopedPlatformHandle(handles->at(0));
        handles->clear();
      }
      delegate_->OnIntroduce(remote_node_name_, data->name,
                             std::move(channel_handle));
      break;
    }

#if defined(OS_WIN) || (defined(OS_MACOSX) && !defined(OS_IOS))
    case MessageType::RELAY_PORTS_MESSAGE: {
      base::ProcessHandle from_process;
      {
        base::AutoLock lock(remote_process_handle_lock_);
        from_process = remote_process_handle_;
      }
      const RelayPortsMessageData* data;
      GetMessagePayload(payload, &data);
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
          break;
        }
      }
#endif
      delegate_->OnRelayPortsMessage(remote_node_name_, from_process,
                                     data->destination, std::move(message));
      break;
    }
#endif

    default:
      DLOG(ERROR) << "Received unknown message type "
                  << static_cast<uint32_t>(header->type) << " from node "
                  << remote_node_name_;
      delegate_->OnChannelError(remote_node_name_);
      break;
  }
}

void NodeChannel::OnChannelError() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  RequestContext request_context(RequestContext::Source::SYSTEM);

  ShutDown();
  // |OnChannelError()| may cause |this| to be destroyed, but still need access
  // to the name name after that destruction. So may a copy of
  // |remote_node_name_| so it can be used if |this| becomes destroyed.
  ports::NodeName node_name = remote_node_name_;
  delegate_->OnChannelError(node_name);
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
  DCHECK(pending_writes.empty() && pending_relays.empty());

  while (!pending_writes.empty()) {
    Channel::MessagePtr message = std::move(pending_writes.front());
    pending_writes.pop();
    if (!relay->SendPortsToProcess(message.get(), remote_process_handle)) {
      LOG(ERROR) << "Error on sending mach ports. Remote process is likely "
                 << "gone. Dropping message.";
      return;
    }

    base::AutoLock lock(channel_lock_);
    if (!channel_) {
      DLOG(ERROR) << "Dropping message on closed channel.";
      break;
    } else {
      channel_->Write(std::move(message));
    }
  }

  while (!pending_relays.empty()) {
    ports::NodeName destination = pending_relays.front().first;
    Channel::MessagePtr message = std::move(pending_relays.front().second);
    pending_relays.pop();
    delegate_->OnRelayPortsMessage(remote_node_name_, remote_process_handle,
                                   destination, std::move(message));
  }
}
#endif

void NodeChannel::WriteChannelMessage(Channel::MessagePtr message) {
#if defined(OS_WIN)
  // Map handles to the destination process. Note: only messages from a
  // privileged node should contain handles on Windows. If an unprivileged
  // node needs to send handles, it should do so via RelayPortsMessage which
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
      if (!message->RewriteHandles(base::GetCurrentProcessHandle(),
                                   remote_process_handle, message->handles(),
                                   message->num_handles())) {
        DLOG(ERROR) << "Failed to duplicate one or more outgoing handles.";
      }
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

      if (!relay->SendPortsToProcess(message.get(), remote_process_handle)) {
        LOG(ERROR) << "Error on sending mach ports. Remote process is likely "
                   << "gone. Dropping message.";
        return;
      }
    }
  }
#endif

  base::AutoLock lock(channel_lock_);
  if (!channel_)
    DLOG(ERROR) << "Dropping message on closed channel.";
  else
    channel_->Write(std::move(message));
}

}  // namespace edk
}  // namespace mojo
