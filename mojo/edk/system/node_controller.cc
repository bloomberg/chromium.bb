// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/node_controller.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "crypto/random.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/platform_support.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/ports_message.h"

namespace mojo {
namespace edk {

namespace {

template <typename T>
void GenerateRandomName(T* out) { crypto::RandBytes(out, sizeof(T)); }

ports::NodeName GetRandomNodeName() {
  ports::NodeName name;
  GenerateRandomName(&name);
  return name;
}

void RecordPeerCount(size_t count) {
  DCHECK_LE(count, static_cast<size_t>(std::numeric_limits<int32_t>::max()));

  // 8k is the maximum number of file descriptors allowed in Chrome.
  UMA_HISTOGRAM_CUSTOM_COUNTS("Mojo.System.Node.ConnectedPeers",
                              static_cast<int32_t>(count),
                              0 /* min */,
                              8000 /* max */,
                              50 /* bucket count */);
}

void RecordPendingChildCount(size_t count) {
  DCHECK_LE(count, static_cast<size_t>(std::numeric_limits<int32_t>::max()));

  // 8k is the maximum number of file descriptors allowed in Chrome.
  UMA_HISTOGRAM_CUSTOM_COUNTS("Mojo.System.Node.PendingChildren",
                              static_cast<int32_t>(count),
                              0 /* min */,
                              8000 /* max */,
                              50 /* bucket count */);
}

// Used by NodeController to watch for shutdown. Since no IO can happen once
// the IO thread is killed, the NodeController can cleanly drop all its peers
// at that time.
class ThreadDestructionObserver :
    public base::MessageLoop::DestructionObserver {
 public:
  static void Create(scoped_refptr<base::TaskRunner> task_runner,
                     const base::Closure& callback) {
    if (task_runner->RunsTasksOnCurrentThread()) {
      // Owns itself.
      new ThreadDestructionObserver(callback);
    } else {
      task_runner->PostTask(FROM_HERE,
                            base::Bind(&Create, task_runner, callback));
    }
  }

 private:
  explicit ThreadDestructionObserver(const base::Closure& callback)
      : callback_(callback) {
    base::MessageLoop::current()->AddDestructionObserver(this);
  }

  ~ThreadDestructionObserver() override {
    base::MessageLoop::current()->RemoveDestructionObserver(this);
  }

  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    callback_.Run();
    delete this;
  }

  const base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(ThreadDestructionObserver);
};

}  // namespace

NodeController::PendingPortRequest::PendingPortRequest() {}

NodeController::PendingPortRequest::~PendingPortRequest() {}

NodeController::ReservedPort::ReservedPort() {}

NodeController::ReservedPort::~ReservedPort() {}

NodeController::PendingRemotePortConnection::PendingRemotePortConnection() {}

NodeController::PendingRemotePortConnection::~PendingRemotePortConnection() {}

NodeController::~NodeController() {}

NodeController::NodeController(Core* core)
    : core_(core),
      name_(GetRandomNodeName()),
      node_(new ports::Node(name_, this)) {
  DVLOG(1) << "Initializing node " << name_;
}

void NodeController::SetIOTaskRunner(
    scoped_refptr<base::TaskRunner> task_runner) {
  io_task_runner_ = task_runner;
  ThreadDestructionObserver::Create(
      io_task_runner_,
      base::Bind(&NodeController::DropAllPeers, base::Unretained(this)));
}

void NodeController::ConnectToChild(base::ProcessHandle process_handle,
                                    ScopedPlatformHandle platform_handle) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NodeController::ConnectToChildOnIOThread,
                 base::Unretained(this),
                 process_handle,
                 base::Passed(&platform_handle)));
}

void NodeController::ConnectToParent(ScopedPlatformHandle platform_handle) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NodeController::ConnectToParentOnIOThread,
                 base::Unretained(this),
                 base::Passed(&platform_handle)));
}

void NodeController::SetPortObserver(
    const ports::PortRef& port,
    const scoped_refptr<PortObserver>& observer) {
  node_->SetUserData(port, observer);
}

void NodeController::ClosePort(const ports::PortRef& port) {
  SetPortObserver(port, nullptr);
  int rv = node_->ClosePort(port);
  DCHECK_EQ(rv, ports::OK) << " Failed to close port: " << port.name();

  AcceptIncomingMessages();
}

int NodeController::SendMessage(const ports::PortRef& port,
                                scoped_ptr<PortsMessage>* message) {
  ports::ScopedMessage ports_message(message->release());
  int rv = node_->SendMessage(port, &ports_message);
  if (rv != ports::OK) {
    DCHECK(ports_message);
    message->reset(static_cast<PortsMessage*>(ports_message.release()));
  }

  AcceptIncomingMessages();
  return rv;
}

void NodeController::ReservePort(const std::string& token,
                                 const ReservePortCallback& callback) {
  ports::PortRef port;
  node_->CreateUninitializedPort(&port);

  DVLOG(2) << "Reserving port " << port.name() << "@" << name_ << " for token "
           << token;

  base::AutoLock lock(reserved_ports_lock_);
  ReservedPort reservation;
  reservation.local_port = port;
  reservation.callback = callback;
  reserved_ports_.insert(std::make_pair(token, reservation));
}

scoped_refptr<PlatformSharedBuffer> NodeController::CreateSharedBuffer(
    size_t num_bytes) {
  // TODO: Broker through the parent over a sync channel. :(
  return internal::g_platform_support->CreateSharedBuffer(num_bytes);
}

void NodeController::ConnectToParentPort(const ports::PortRef& local_port,
                                         const std::string& token,
                                         const base::Closure& callback) {
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NodeController::RequestParentPortConnectionOnIOThread,
                 base::Unretained(this), local_port, token, callback));
}

void NodeController::ConnectToRemotePort(
    const ports::PortRef& local_port,
    const ports::NodeName& remote_node_name,
    const ports::PortName& remote_port_name,
    const base::Closure& callback) {
  if (remote_node_name == name_) {
    // It's possible that two different code paths on the node are trying to
    // bootstrap ports to each other (e.g. in Chrome single-process mode)
    // without being aware of the fact. In this case we can initialize the port
    // immediately (which can fail silently if it's already been initialized by
    // the request on the other side), and invoke |callback|.
    node_->InitializePort(local_port, name_, remote_port_name);
    callback.Run();
    return;
  }

  PendingRemotePortConnection connection;
  connection.local_port = local_port;
  connection.remote_node_name = remote_node_name;
  connection.remote_port_name = remote_port_name;
  connection.callback = callback;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NodeController::ConnectToRemotePortOnIOThread,
                 base::Unretained(this), connection));
}

void NodeController::RequestShutdown(const base::Closure& callback) {
  {
    base::AutoLock lock(shutdown_lock_);
    shutdown_callback_ = callback;
  }

  AttemptShutdownIfRequested();
}

void NodeController::ConnectToChildOnIOThread(
    base::ProcessHandle process_handle,
    ScopedPlatformHandle platform_handle) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  scoped_refptr<NodeChannel> channel =
      NodeChannel::Create(this, std::move(platform_handle), io_task_runner_);

  ports::NodeName token;
  GenerateRandomName(&token);

  channel->SetRemoteNodeName(token);
  channel->SetRemoteProcessHandle(process_handle);
  channel->Start();
  channel->AcceptChild(name_, token);

  pending_children_.insert(std::make_pair(token, channel));

  RecordPendingChildCount(pending_children_.size());
}

void NodeController::ConnectToParentOnIOThread(
    ScopedPlatformHandle platform_handle) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  base::AutoLock lock(parent_lock_);
  DCHECK(parent_name_ == ports::kInvalidNodeName);

  // At this point we don't know the parent's name, so we can't yet insert it
  // into our |peers_| map. That will happen as soon as we receive an
  // AcceptChild message from them.
  bootstrap_parent_channel_ =
      NodeChannel::Create(this, std::move(platform_handle), io_task_runner_);
  bootstrap_parent_channel_->Start();
}

void NodeController::RequestParentPortConnectionOnIOThread(
    const ports::PortRef& local_port,
    const std::string& token,
    const base::Closure& callback) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  scoped_refptr<NodeChannel> parent = GetParentChannel();
  if (!parent) {
    PendingPortRequest request;
    request.token = token;
    request.local_port = local_port;
    request.callback = callback;
    pending_port_requests_.push_back(request);
    return;
  }

  pending_parent_port_connections_.insert(
      std::make_pair(local_port.name(), callback));
  parent->RequestPortConnection(local_port.name(), token);
}

void NodeController::ConnectToRemotePortOnIOThread(
    const PendingRemotePortConnection& connection) {
  scoped_refptr<NodeChannel> peer = GetPeerChannel(connection.remote_node_name);
  if (peer) {
    // It's safe to initialize the port since we already have a channel to its
    // peer. No need to actually send them a message.
    int rv = node_->InitializePort(connection.local_port,
                                   connection.remote_node_name,
                                   connection.remote_port_name);
    DCHECK_EQ(rv, ports::OK);
    connection.callback.Run();
    return;
  }

  // Save this for later. We'll initialize the port once this:: peer is added.
  pending_remote_port_connections_[connection.remote_node_name].push_back(
      connection);
}

scoped_refptr<NodeChannel> NodeController::GetPeerChannel(
    const ports::NodeName& name) {
  base::AutoLock lock(peers_lock_);
  auto it = peers_.find(name);
  if (it == peers_.end())
    return nullptr;
  return it->second;
}

scoped_refptr<NodeChannel> NodeController::GetParentChannel() {
  ports::NodeName parent_name;
  {
    base::AutoLock lock(parent_lock_);
    parent_name = parent_name_;
  }
  return GetPeerChannel(parent_name);
}

void NodeController::AddPeer(const ports::NodeName& name,
                             scoped_refptr<NodeChannel> channel,
                             bool start_channel) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  DCHECK(name != ports::kInvalidNodeName);
  DCHECK(channel);

  channel->SetRemoteNodeName(name);

  OutgoingMessageQueue pending_messages;
  {
    base::AutoLock lock(peers_lock_);
    if (peers_.find(name) != peers_.end()) {
      // This can happen normally if two nodes race to be introduced to each
      // other. The losing pipe will be silently closed and introduction should
      // not be affected.
      DVLOG(1) << "Ignoring duplicate peer name " << name;
      return;
    }

    auto result = peers_.insert(std::make_pair(name, channel));
    DCHECK(result.second);

    DVLOG(2) << "Accepting new peer " << name << " on node " << name_;

    RecordPeerCount(peers_.size());

    auto it = pending_peer_messages_.find(name);
    if (it != pending_peer_messages_.end()) {
      std::swap(pending_messages, it->second);
      pending_peer_messages_.erase(it);
    }
  }

  if (start_channel)
    channel->Start();

  // Flush any queued message we need to deliver to this node.
  while (!pending_messages.empty()) {
    ports::ScopedMessage message = std::move(pending_messages.front());
    channel->PortsMessage(
        static_cast<PortsMessage*>(message.get())->TakeChannelMessage());
    pending_messages.pop();
  }

  // Complete any pending port connections to this peer.
  auto connections_it = pending_remote_port_connections_.find(name);
  if (connections_it != pending_remote_port_connections_.end()) {
    for (const auto& connection : connections_it->second) {
      int rv = node_->InitializePort(connection.local_port,
                                     connection.remote_node_name,
                                     connection.remote_port_name);
      DCHECK_EQ(rv, ports::OK);
      connection.callback.Run();
    }
    pending_remote_port_connections_.erase(connections_it);
  }
}

void NodeController::DropPeer(const ports::NodeName& name) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  {
    base::AutoLock lock(peers_lock_);
    auto it = peers_.find(name);

    if (it != peers_.end()) {
      ports::NodeName peer = it->first;
      peers_.erase(it);
      DVLOG(1) << "Dropped peer " << peer;
    }

    pending_peer_messages_.erase(name);
    pending_children_.erase(name);

    RecordPeerCount(peers_.size());
    RecordPendingChildCount(pending_children_.size());
  }

  node_->LostConnectionToNode(name);
}

void NodeController::SendPeerMessage(const ports::NodeName& name,
                                     ports::ScopedMessage message) {
  PortsMessage* ports_message = static_cast<PortsMessage*>(message.get());

#if defined(OS_WIN)
  // If we're sending a message with handles and we're not the parent,
  // relay the message through the parent.
  if (ports_message->has_handles()) {
    scoped_refptr<NodeChannel> parent = GetParentChannel();
    if (parent) {
      parent->RelayPortsMessage(name, ports_message->TakeChannelMessage());
      return;
    }
  }
#endif

  scoped_refptr<NodeChannel> peer = GetPeerChannel(name);
  if (peer) {
    peer->PortsMessage(ports_message->TakeChannelMessage());
    return;
  }

  // If we don't know who the peer is, queue the message for delivery. If this
  // is the first message queued for the peer, we also ask the parent to
  // introduce us to them.

  bool needs_introduction = false;
  {
    base::AutoLock lock(peers_lock_);
    auto& queue = pending_peer_messages_[name];
    needs_introduction = queue.empty();
    queue.emplace(std::move(message));
  }

  if (needs_introduction) {
    scoped_refptr<NodeChannel> parent = GetParentChannel();
    if (!parent) {
      DVLOG(1) << "Dropping message for unknown peer: " << name;
      return;
    }
    parent->RequestIntroduction(name);
  }
}

void NodeController::AcceptIncomingMessages() {
  std::queue<ports::ScopedMessage> messages;
  for (;;) {
    // TODO: We may need to be more careful to avoid starving the rest of the
    // thread here. Revisit this if it turns out to be a problem. One
    // alternative would be to schedule a task to continue pumping messages
    // after flushing once.

    {
      base::AutoLock lock(messages_lock_);
      if (incoming_messages_.empty())
        break;
      std::swap(messages, incoming_messages_);
    }

    while (!messages.empty()) {
      node_->AcceptMessage(std::move(messages.front()));
      messages.pop();
    }
  }
  AttemptShutdownIfRequested();
}

void NodeController::DropAllPeers() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  {
    base::AutoLock lock(parent_lock_);
    if (bootstrap_parent_channel_) {
      bootstrap_parent_channel_->ShutDown();
      bootstrap_parent_channel_ = nullptr;
    }
  }

  std::vector<scoped_refptr<NodeChannel>> all_peers;
  {
    base::AutoLock lock(peers_lock_);
    for (const auto& peer : peers_)
      all_peers.push_back(peer.second);
    for (const auto& peer : pending_children_)
      all_peers.push_back(peer.second);
    peers_.clear();
    pending_children_.clear();
    pending_peer_messages_.clear();
  }

  for (const auto& peer : all_peers)
    peer->ShutDown();

  if (destroy_on_io_thread_shutdown_)
    delete this;
}

void NodeController::GenerateRandomPortName(ports::PortName* port_name) {
  GenerateRandomName(port_name);
}

void NodeController::AllocMessage(size_t num_header_bytes,
                                  ports::ScopedMessage* message) {
  message->reset(new PortsMessage(num_header_bytes, 0, 0, nullptr));
}

void NodeController::ForwardMessage(const ports::NodeName& node,
                                    ports::ScopedMessage message) {
  if (node == name_) {
    // NOTE: We need to avoid re-entering the Node instance within
    // ForwardMessage. Because ForwardMessage is only ever called
    // (synchronously) in response to Node's ClosePort, SendMessage, or
    // AcceptMessage, we flush the queue after calling any of those methods.
    base::AutoLock lock(messages_lock_);
    incoming_messages_.emplace(std::move(message));
  } else {
    SendPeerMessage(node, std::move(message));
  }
}

void NodeController::PortStatusChanged(const ports::PortRef& port) {
  scoped_refptr<ports::UserData> user_data;
  node_->GetUserData(port, &user_data);

  PortObserver* observer = static_cast<PortObserver*>(user_data.get());
  if (observer) {
    observer->OnPortStatusChanged();
  } else {
    DVLOG(2) << "Ignoring status change for " << port.name() << " because it "
             << "doesn't have an observer.";
  }
}

void NodeController::OnAcceptChild(const ports::NodeName& from_node,
                                   const ports::NodeName& parent_name,
                                   const ports::NodeName& token) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  scoped_refptr<NodeChannel> parent;
  {
    base::AutoLock lock(parent_lock_);
    if (!bootstrap_parent_channel_ || parent_name_ != ports::kInvalidNodeName) {
      DLOG(ERROR) << "Unexpected AcceptChild message from " << from_node;
      DropPeer(from_node);
      return;
    }

    parent_name_ = parent_name;
    parent = bootstrap_parent_channel_;
    bootstrap_parent_channel_ = nullptr;
  }

  parent->AcceptParent(token, name_);
  for (const auto& request : pending_port_requests_) {
    pending_parent_port_connections_.insert(
        std::make_pair(request.local_port.name(), request.callback));
    parent->RequestPortConnection(request.local_port.name(), request.token);
  }
  pending_port_requests_.clear();

  DVLOG(1) << "Child " << name_ << " accepting parent " << parent_name;

  AddPeer(parent_name_, parent, false /* start_channel */);
}

void NodeController::OnAcceptParent(const ports::NodeName& from_node,
                                    const ports::NodeName& token,
                                    const ports::NodeName& child_name) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  auto it = pending_children_.find(from_node);
  if (it == pending_children_.end() || token != from_node) {
    DLOG(ERROR) << "Received unexpected AcceptParent message from "
                << from_node;
    DropPeer(from_node);
    return;
  }

  scoped_refptr<NodeChannel> channel = it->second;
  pending_children_.erase(it);

  DCHECK(channel);

  DVLOG(1) << "Parent " << name_ << " accepted child " << child_name;

  // If the child has a grandparent, we want to make sure they're introduced
  // as well. The grandparent will be sent a named channel handle, then we'll
  // add the child as our peer, then we'll introduce the child to the parent.

  scoped_refptr<NodeChannel> parent = GetParentChannel();
  ports::NodeName parent_name;
  scoped_ptr<PlatformChannelPair> grandparent_channel;
  if (parent) {
    base::AutoLock lock(parent_lock_);
    parent_name = parent_name_;
    grandparent_channel.reset(new PlatformChannelPair);
  }

  if (grandparent_channel)
    parent->Introduce(child_name, grandparent_channel->PassServerHandle());

  AddPeer(child_name, channel, false /* start_channel */);

  if (grandparent_channel)
    channel->Introduce(parent_name, grandparent_channel->PassClientHandle());
}

void NodeController::OnPortsMessage(Channel::MessagePtr channel_message) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  void* data;
  size_t num_data_bytes;
  NodeChannel::GetPortsMessageData(
      channel_message.get(), &data, &num_data_bytes);

  size_t num_header_bytes, num_payload_bytes, num_ports_bytes;
  ports::Message::Parse(data,
                        num_data_bytes,
                        &num_header_bytes,
                        &num_payload_bytes,
                        &num_ports_bytes);

  CHECK(channel_message);
  ports::ScopedMessage message(
      new PortsMessage(num_header_bytes,
                       num_payload_bytes,
                       num_ports_bytes,
                       std::move(channel_message)));

  node_->AcceptMessage(std::move(message));
  AcceptIncomingMessages();
  AttemptShutdownIfRequested();
}

void NodeController::OnRequestPortConnection(
    const ports::NodeName& from_node,
    const ports::PortName& connector_port_name,
    const std::string& token) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  DVLOG(2) << "Node " << name_ << " received RequestPortConnection for token "
           << token << " and port " << connector_port_name << "@" << from_node;

  ReservePortCallback callback;
  ports::PortRef local_port;
  {
    base::AutoLock lock(reserved_ports_lock_);
    auto it = reserved_ports_.find(token);
    if (it == reserved_ports_.end()) {
      DVLOG(1) << "Ignoring request to connect to port for unknown token "
               << token;
      return;
    }
    local_port = it->second.local_port;
    callback = it->second.callback;
    reserved_ports_.erase(it);
  }

  DCHECK(!callback.is_null());

  scoped_refptr<NodeChannel> peer = GetPeerChannel(from_node);
  if (!peer) {
    DVLOG(1) << "Ignoring request to connect to port from unknown node "
             << from_node;
    return;
  }

  // This reserved port should not have been initialized yet.
  CHECK_EQ(ports::OK, node_->InitializePort(local_port, from_node,
                                            connector_port_name));

  peer->ConnectToPort(local_port.name(), connector_port_name);
  callback.Run(local_port);
}

void NodeController::OnConnectToPort(
    const ports::NodeName& from_node,
    const ports::PortName& connector_port_name,
    const ports::PortName& connectee_port_name) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  DVLOG(2) << "Node " << name_ << " received ConnectToPort for local port "
           << connectee_port_name << " to port " << connector_port_name << "@"
           << from_node;

  ports::PortRef connectee_port;
  int rv = node_->GetPort(connectee_port_name, &connectee_port);
  if (rv != ports::OK) {
    DLOG(ERROR) << "Ignoring ConnectToPort for unknown port "
                << connectee_port_name;
    return;
  }

  // It's OK if this port has already been initialized. This message is only
  // sent by the remote peer to ensure the port is ready before it starts
  // us sending messages to it.
  ports::PortStatus port_status;
  rv = node_->GetStatus(connectee_port, &port_status);
  if (rv == ports::OK) {
    DVLOG(1) << "Ignoring ConnectToPort for already-initialized port "
             << connectee_port_name;
    return;
  }

  CHECK_EQ(ports::OK, node_->InitializePort(connectee_port, from_node,
                                            connector_port_name));

  auto it = pending_parent_port_connections_.find(connectee_port_name);
  DCHECK(it != pending_parent_port_connections_.end());
  it->second.Run();
  pending_parent_port_connections_.erase(it);
}

void NodeController::OnRequestIntroduction(const ports::NodeName& from_node,
                                           const ports::NodeName& name) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  scoped_refptr<NodeChannel> requestor = GetPeerChannel(from_node);
  if (from_node == name || name == ports::kInvalidNodeName || !requestor) {
    DLOG(ERROR) << "Rejecting invalid OnRequestIntroduction message from "
                << from_node;
    DropPeer(from_node);
    return;
  }

  scoped_refptr<NodeChannel> new_friend = GetPeerChannel(name);
  if (!new_friend) {
    // We don't know who they're talking about!
    requestor->Introduce(name, ScopedPlatformHandle());
  } else {
    PlatformChannelPair new_channel;
    requestor->Introduce(name, new_channel.PassServerHandle());
    new_friend->Introduce(from_node, new_channel.PassClientHandle());
  }
}

void NodeController::OnIntroduce(const ports::NodeName& from_node,
                                 const ports::NodeName& name,
                                 ScopedPlatformHandle channel_handle) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  if (!channel_handle.is_valid()) {
    DLOG(ERROR) << "Could not be introduced to peer " << name;
    base::AutoLock lock(peers_lock_);
    pending_peer_messages_.erase(name);
    return;
  }

  scoped_refptr<NodeChannel> channel =
      NodeChannel::Create(this, std::move(channel_handle), io_task_runner_);

  DVLOG(1) << "Adding new peer " << name << " via parent introduction.";
  AddPeer(name, channel, true /* start_channel */);
}

#if defined(OS_WIN)
void NodeController::OnRelayPortsMessage(const ports::NodeName& from_node,
                                         base::ProcessHandle from_process,
                                         const ports::NodeName& destination,
                                         Channel::MessagePtr message) {
  scoped_refptr<NodeChannel> parent = GetParentChannel();
  if (parent) {
    // Only the parent should be asked to relay a message.
    DLOG(ERROR) << "Non-parent refusing to relay message.";
    DropPeer(from_node);
    return;
  }

  // The parent should always know which process this came from.
  DCHECK(from_process != base::kNullProcessHandle);

  // Duplicate the handles to this (the parent) process. If the message is
  // destined for another child process, the handles will be duplicated to
  // that process before going out (see NodeChannel::WriteChannelMessage).
  //
  // TODO: We could avoid double-duplication.
  for (size_t i = 0; i < message->num_handles(); ++i) {
    BOOL result = DuplicateHandle(
        from_process, message->handles()[i].handle,
        base::GetCurrentProcessHandle(),
        reinterpret_cast<HANDLE*>(message->handles() + i),
        0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
    DCHECK(result);
  }
  if (destination == name_) {
    // Great, we can deliver this message locally.
    OnPortsMessage(std::move(message));
    return;
  }

  scoped_refptr<NodeChannel> peer = GetPeerChannel(destination);
  if (peer)
    peer->PortsMessage(std::move(message));
  else
    DLOG(ERROR) << "Dropping relay message for unknown node " << destination;
}
#endif

void NodeController::OnChannelError(const ports::NodeName& from_node) {
  if (io_task_runner_->RunsTasksOnCurrentThread()) {
    DropPeer(from_node);
  } else {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&NodeController::DropPeer, base::Unretained(this),
                   from_node));
  }
}

void NodeController::DestroyOnIOThreadShutdown() {
  destroy_on_io_thread_shutdown_ = true;
}

void NodeController::AttemptShutdownIfRequested() {
  base::Closure callback;
  {
    base::AutoLock lock(shutdown_lock_);
    if (shutdown_callback_.is_null())
      return;
    if (!node_->CanShutdownCleanly(true /* allow_local_ports */)) {
      DVLOG(2) << "Unable to cleanly shut down node " << name_ << ".";
      return;
    }
    callback = shutdown_callback_;
    shutdown_callback_.Reset();
  }

  DCHECK(!callback.is_null());

  callback.Run();
}

}  // namespace edk
}  // namespace mojo
