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
#include "base/process/process_handle.h"
#include "crypto/random.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/system/broker.h"
#include "mojo/edk/system/broker_host.h"
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
// TODO(amistry): Consider the need for a broker on Windows.
#if defined(OS_POSIX)
  // On posix, use the bootstrap channel for the broker and receive the node's
  // channel synchronously as the first message from the broker.
  broker_.reset(new Broker(std::move(platform_handle)));
  platform_handle = broker_->GetParentPlatformHandle();
#endif

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
                                 const ports::PortRef& port) {
  DVLOG(2) << "Reserving port " << port.name() << "@" << name_ << " for token "
           << token;

  base::AutoLock lock(reserved_ports_lock_);
  auto result = reserved_ports_.insert(std::make_pair(token, port));
  DCHECK(result.second);
}

void NodeController::MergePortIntoParent(const std::string& token,
                                         const ports::PortRef& port) {
  scoped_refptr<NodeChannel> parent = GetParentChannel();
  if (parent) {
    parent->RequestPortMerge(port.name(), token);
    return;
  }

  base::AutoLock lock(pending_port_merges_lock_);
  pending_port_merges_.push_back(std::make_pair(token, port));
}

scoped_refptr<PlatformSharedBuffer> NodeController::CreateSharedBuffer(
    size_t num_bytes) {
  scoped_refptr<PlatformSharedBuffer> buffer =
      PlatformSharedBuffer::Create(num_bytes);
#if defined(OS_POSIX)
  if (!buffer && broker_) {
    // On POSIX, creating a shared buffer in a sandboxed process will fail, so
    // fall back to the broker if there is one.
    buffer = broker_->GetSharedBuffer(num_bytes);
  }
#endif
  return buffer;
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

#if defined(OS_POSIX)
  PlatformChannelPair node_channel;
  // BrokerHost owns itself.
  BrokerHost* broker_host = new BrokerHost(std::move(platform_handle));
  broker_host->SendChannel(node_channel.PassClientHandle());
  scoped_refptr<NodeChannel> channel = NodeChannel::Create(
      this, node_channel.PassServerHandle(), io_task_runner_);
#else
  scoped_refptr<NodeChannel> channel =
      NodeChannel::Create(this, std::move(platform_handle), io_task_runner_);
#endif

  // We set up the child channel with a temporary name so it can be identified
  // as a pending child if it writes any messages to the channel. We may start
  // receiving messages from it (though we shouldn't) as soon as Start() is
  // called below.
  ports::NodeName token;
  GenerateRandomName(&token);

  pending_children_.insert(std::make_pair(token, channel));
  RecordPendingChildCount(pending_children_.size());

  channel->SetRemoteNodeName(token);
  channel->SetRemoteProcessHandle(process_handle);
  channel->Start();

  channel->AcceptChild(name_, token);
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

scoped_refptr<NodeChannel> NodeController::GetBrokerChannel() {
  ports::NodeName broker_name;
  {
    base::AutoLock lock(broker_lock_);
    broker_name = broker_name_;
  }
  return GetPeerChannel(broker_name);
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
    channel->PortsMessage(std::move(pending_messages.front()));
    pending_messages.pop();
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
  Channel::MessagePtr channel_message =
      static_cast<PortsMessage*>(message.get())->TakeChannelMessage();

  scoped_refptr<NodeChannel> peer = GetPeerChannel(name);
#if defined(OS_WIN)
  if (channel_message->has_handles()) {
    // If we're sending a message with handles we aren't the destination
    // node's parent or broker (i.e. we don't know its process handle), ask
    // the broker to relay for us.
    scoped_refptr<NodeChannel> broker = GetBrokerChannel();
    if (!peer || !peer->HasRemoteProcessHandle()) {
      if (broker) {
        broker->RelayPortsMessage(name, std::move(channel_message));
      } else {
        base::AutoLock lock(broker_lock_);
        pending_relay_messages_[name].emplace(std::move(channel_message));
      }
      return;
    }
  }
#endif

  if (peer) {
    peer->PortsMessage(std::move(channel_message));
    return;
  }

  // If we don't know who the peer is, queue the message for delivery. If this
  // is the first message queued for the peer, we also ask the broker to
  // introduce us to them.

  bool needs_introduction = false;
  {
    base::AutoLock lock(peers_lock_);
    auto& queue = pending_peer_messages_[name];
    needs_introduction = queue.empty();
    queue.emplace(std::move(channel_message));
  }

  if (needs_introduction) {
    scoped_refptr<NodeChannel> broker = GetBrokerChannel();
    if (!broker) {
      DVLOG(1) << "Dropping message for unknown peer: " << name;
      return;
    }
    broker->RequestIntroduction(name);
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
  }

  parent->SetRemoteNodeName(parent_name);
  parent->AcceptParent(token, name_);

  // NOTE: The child does not actually add its parent as a peer until
  // receiving an AcceptBrokerClient message from the broker. The parent
  // will request that said message be sent upon receiving AcceptParent.

  DVLOG(1) << "Child " << name_ << " accepting parent " << parent_name;
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

  AddPeer(child_name, channel, false /* start_channel */);

  // TODO(rockot/amistry): We could simplify child initialization if we could
  // synchronously get a new async broker channel from the broker. For now we do
  // it asynchronously since it's only used to facilitate handle passing, not
  // handle creation.
  scoped_refptr<NodeChannel> broker = GetBrokerChannel();
  if (broker) {
    // Inform the broker of this new child.
    broker->AddBrokerClient(child_name, channel->CopyRemoteProcessHandle());
  } else {
    // If we have no broker, either we need to wait for one, or we *are* the
    // broker.
    scoped_refptr<NodeChannel> parent = GetParentChannel();
    if (!parent) {
      base::AutoLock lock(parent_lock_);
      parent = bootstrap_parent_channel_;
    }

    if (!parent) {
      // Yes, we're the broker. We can initialize the child directly.
      channel->AcceptBrokerClient(name_, ScopedPlatformHandle());
    } else {
      // We aren't the broker, so wait for a broker connection.
      base::AutoLock lock(broker_lock_);
      pending_broker_clients_.push(child_name);
    }
  }
}

void NodeController::OnAddBrokerClient(const ports::NodeName& from_node,
                                       const ports::NodeName& client_name,
                                       ScopedPlatformHandle process_handle) {
  scoped_refptr<NodeChannel> sender = GetPeerChannel(from_node);
  if (!sender) {
    DLOG(ERROR) << "Ignoring AddBrokerClient from unknown sender.";
    return;
  }

  if (GetPeerChannel(client_name)) {
    DLOG(ERROR) << "Ignoring AddBrokerClient for known client.";
    DropPeer(from_node);
    return;
  }

  PlatformChannelPair broker_channel;
  scoped_refptr<NodeChannel> client = NodeChannel::Create(
      this, broker_channel.PassServerHandle(), io_task_runner_);

#if defined(OS_WIN)
  // The broker must have a working handle to the client process in order to
  // properly copy other handles to and from the client.
  if(!process_handle.is_valid()) {
    DLOG(ERROR) << "Broker rejecting client with invalid process handle.";
    return;
  }
  client->SetRemoteProcessHandle(process_handle.release().handle);
#endif

  AddPeer(client_name, client, true /* start_channel */);

  DVLOG(1) << "Broker " << name_ << " accepting client " << client_name
           << " from peer " << from_node;

  sender->BrokerClientAdded(client_name, broker_channel.PassClientHandle());
}

void NodeController::OnBrokerClientAdded(const ports::NodeName& from_node,
                                         const ports::NodeName& client_name,
                                         ScopedPlatformHandle broker_channel) {
  scoped_refptr<NodeChannel> client = GetPeerChannel(client_name);
  if (!client) {
    DLOG(ERROR) << "BrokerClientAdded for unknown child " << client_name;
    return;
  }

  // This should have come from our own broker.
  if(GetBrokerChannel() != GetPeerChannel(from_node)) {
    DLOG(ERROR) << "BrokerClientAdded from non-broker node " << from_node;
    return;
  }

  DVLOG(1) << "Child " << client_name << " accepted by broker " << from_node;

  client->AcceptBrokerClient(from_node, std::move(broker_channel));
}

void NodeController::OnAcceptBrokerClient(const ports::NodeName& from_node,
                                          const ports::NodeName& broker_name,
                                          ScopedPlatformHandle broker_channel) {
  // This node should already have a parent in bootstrap mode.
  ports::NodeName parent_name;
  scoped_refptr<NodeChannel> parent;
  {
    base::AutoLock lock(parent_lock_);
    parent_name = parent_name_;
    parent = bootstrap_parent_channel_;
    bootstrap_parent_channel_ = nullptr;
  }
  DCHECK(parent_name == from_node);
  DCHECK(parent);

  std::queue<ports::NodeName> pending_broker_clients;
  std::unordered_map<ports::NodeName, OutgoingMessageQueue>
      pending_relay_messages;
  {
    base::AutoLock lock(broker_lock_);
    broker_name_ = broker_name;
    std::swap(pending_broker_clients, pending_broker_clients_);
    std::swap(pending_relay_messages, pending_relay_messages_);
  }
  DCHECK(broker_name != ports::kInvalidNodeName);

  // It's now possible to add both the broker and the parent as peers.
  // Note that the broker and parent may be the same node.
  scoped_refptr<NodeChannel> broker;
  if (broker_name == parent_name) {
    DCHECK(!broker_channel.is_valid());
    broker = parent;
  } else {
    DCHECK(broker_channel.is_valid());
    broker = NodeChannel::Create(this, std::move(broker_channel),
                                 io_task_runner_);
    AddPeer(broker_name, broker, true /* start_channel */);
  }

  AddPeer(parent_name, parent, false /* start_channel */);

  {
    // Complete any port merge requests we have waiting for the parent.
    base::AutoLock lock(pending_port_merges_lock_);
    for (const auto& request : pending_port_merges_)
      parent->RequestPortMerge(request.second.name(), request.first);
    pending_port_merges_.clear();
  }

  // Feed the broker any pending children of our own.
  while (!pending_broker_clients.empty()) {
    const ports::NodeName& child_name = pending_broker_clients.front();
    auto it = pending_children_.find(child_name);
    DCHECK(it != pending_children_.end());
    broker->AddBrokerClient(child_name, it->second->CopyRemoteProcessHandle());
    pending_broker_clients.pop();
  }

#if defined(OS_WIN)
  // Have the broker relay any messages we have waiting.
  for (auto& entry : pending_relay_messages) {
    const ports::NodeName& destination = entry.first;
    auto& message_queue = entry.second;
    while (!message_queue.empty()) {
      broker->RelayPortsMessage(destination, std::move(message_queue.front()));
      message_queue.pop();
    }
  }
#endif

  DVLOG(1) << "Child " << name_ << " accepted by broker " << broker_name;
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

void NodeController::OnRequestPortMerge(
    const ports::NodeName& from_node,
    const ports::PortName& connector_port_name,
    const std::string& token) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  DVLOG(2) << "Node " << name_ << " received RequestPortMerge for token "
           << token << " and port " << connector_port_name << "@" << from_node;

  ports::PortRef local_port;
  {
    base::AutoLock lock(reserved_ports_lock_);
    auto it = reserved_ports_.find(token);
    if (it == reserved_ports_.end()) {
      DVLOG(1) << "Ignoring request to connect to port for unknown token "
               << token;
      return;
    }
    local_port = it->second;
  }

  int rv = node_->MergePorts(local_port, from_node, connector_port_name);
  if (rv != ports::OK)
    DLOG(ERROR) << "MergePorts failed: " << rv;
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
  if (GetBrokerChannel()) {
    // Only the broker should be asked to relay a message.
    LOG(ERROR) << "Non-broker refusing to relay message.";
    DropPeer(from_node);
    return;
  }

  // The parent should always know which process this came from.
  DCHECK(from_process != base::kNullProcessHandle);

  // Rewrite the handles to this (the parent) process. If the message is
  // destined for another child process, the handles will be rewritten to that
  // process before going out (see NodeChannel::WriteChannelMessage).
  //
  // TODO: We could avoid double-duplication.
  if (!Channel::Message::RewriteHandles(from_process,
                                        base::GetCurrentProcessHandle(),
                                        message->handles(),
                                        message->num_handles())) {
    DLOG(ERROR) << "Failed to relay one or more handles.";
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
