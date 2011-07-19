// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/p2p_transport_impl.h"

#include "base/values.h"
#include "content/common/json_value_serializer.h"
#include "content/renderer/p2p/ipc_network_manager.h"
#include "content/renderer/p2p/ipc_socket_factory.h"
#include "content/renderer/render_view.h"
#include "jingle/glue/channel_socket_adapter.h"
#include "jingle/glue/pseudotcp_adapter.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/base/net_errors.h"
#include "third_party/libjingle/source/talk/p2p/base/p2ptransportchannel.h"
#include "third_party/libjingle/source/talk/p2p/client/basicportallocator.h"

P2PTransportImpl::P2PTransportImpl(
    talk_base::NetworkManager* network_manager,
    talk_base::PacketSocketFactory* socket_factory)
    : event_handler_(NULL),
      state_(STATE_NONE),
      network_manager_(network_manager),
      socket_factory_(socket_factory),
      ALLOW_THIS_IN_INITIALIZER_LIST(connect_callback_(
          this, &P2PTransportImpl::OnTcpConnected)) {
}

P2PTransportImpl::P2PTransportImpl(P2PSocketDispatcher* socket_dispatcher)
    : event_handler_(NULL),
      state_(STATE_NONE),
      network_manager_(new IpcNetworkManager(socket_dispatcher)),
      socket_factory_(new IpcPacketSocketFactory(socket_dispatcher)),
      ALLOW_THIS_IN_INITIALIZER_LIST(connect_callback_(
          this, &P2PTransportImpl::OnTcpConnected)) {
}

P2PTransportImpl::~P2PTransportImpl() {
}

bool P2PTransportImpl::Init(const std::string& name,
                            Protocol protocol,
                            const std::string& config,
                            EventHandler* event_handler) {
  DCHECK(event_handler);

  // Before proceeding, ensure we have libjingle thread wrapper for
  // the current thread.
  jingle_glue::JingleThreadWrapper::EnsureForCurrentThread();

  name_ = name;
  event_handler_ = event_handler;

  // TODO(sergeyu): Implement PortAllocator that can parse |config|
  // and use it here instead of BasicPortAllocator.
  allocator_.reset(new cricket::BasicPortAllocator(
      network_manager_.get(), socket_factory_.get()));

  DCHECK(!channel_.get());
  channel_.reset(new cricket::P2PTransportChannel(
      name, "", NULL, allocator_.get()));
  channel_->SignalRequestSignaling.connect(
      this, &P2PTransportImpl::OnRequestSignaling);
  channel_->SignalCandidateReady.connect(
      this, &P2PTransportImpl::OnCandidateReady);

  if (protocol == PROTOCOL_UDP) {
    channel_->SignalWritableState.connect(
        this, &P2PTransportImpl::OnReadableState);
    channel_->SignalWritableState.connect(
        this, &P2PTransportImpl::OnWriteableState);
  }

  channel_adapter_.reset(new jingle_glue::TransportChannelSocketAdapter(
      channel_.get()));

  channel_->Connect();

  if (protocol == PROTOCOL_TCP) {
    pseudo_tcp_adapter_.reset(new jingle_glue::PseudoTcpAdapter(
        channel_adapter_.release()));
    int result = pseudo_tcp_adapter_->Connect(&connect_callback_);
    if (result != net::ERR_IO_PENDING)
      OnTcpConnected(result);
  }

  return true;
}

bool P2PTransportImpl::AddRemoteCandidate(const std::string& address) {
  cricket::Candidate candidate;
  if (!DeserializeCandidate(address, &candidate)) {
    return false;
  }

  channel_->OnCandidate(candidate);
  return true;
}

void P2PTransportImpl::OnRequestSignaling() {
  channel_->OnSignalingReady();
}

void P2PTransportImpl::OnCandidateReady(
    cricket::TransportChannelImpl* channel,
    const cricket::Candidate& candidate) {
  event_handler_->OnCandidateReady(SerializeCandidate(candidate));
}

void P2PTransportImpl::OnReadableState(cricket::TransportChannel* channel) {
  state_ = static_cast<State>(state_ | STATE_READABLE);
  event_handler_->OnStateChange(state_);
}

void P2PTransportImpl::OnWriteableState(cricket::TransportChannel* channel) {
  state_ = static_cast<State>(state_ | STATE_WRITABLE);
  event_handler_->OnStateChange(state_);
}

std::string P2PTransportImpl::SerializeCandidate(
    const cricket::Candidate& candidate) {

  // TODO(sergeyu): Use SDP to format candidates?
  DictionaryValue value;
  value.SetString("name", candidate.name());
  value.SetString("ip", candidate.address().IPAsString());
  value.SetInteger("port", candidate.address().port());
  value.SetString("type", candidate.type());
  value.SetString("protocol", candidate.protocol());
  value.SetString("username", candidate.username());
  value.SetString("password", candidate.password());
  value.SetDouble("preference", candidate.preference());
  value.SetInteger("generation", candidate.generation());

  std::string result;
  JSONStringValueSerializer serializer(&result);
  serializer.Serialize(value);
  return result;
}

bool P2PTransportImpl::DeserializeCandidate(const std::string& address,
                                            cricket::Candidate* candidate) {
  JSONStringValueSerializer deserializer(address);
  scoped_ptr<Value> value(deserializer.Deserialize(NULL, NULL));
  if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY)) {
    return false;
  }

  DictionaryValue* dic_value = static_cast<DictionaryValue*>(value.get());

  std::string name;
  std::string ip;
  int port;
  std::string type;
  std::string protocol;
  std::string username;
  std::string password;
  double preference;
  int generation;

  if (!dic_value->GetString("name", &name) ||
      !dic_value->GetString("ip", &ip) ||
      !dic_value->GetInteger("port", &port) ||
      !dic_value->GetString("type", &type) ||
      !dic_value->GetString("protocol", &protocol) ||
      !dic_value->GetString("username", &username) ||
      !dic_value->GetString("password", &password) ||
      !dic_value->GetDouble("preference", &preference) ||
      !dic_value->GetInteger("generation", &generation)) {
    return false;
  }

  candidate->set_name(name);
  candidate->set_address(talk_base::SocketAddress(ip, port));
  candidate->set_type(type);
  candidate->set_protocol(protocol);
  candidate->set_username(username);
  candidate->set_password(password);
  candidate->set_preference(static_cast<float>(preference));
  candidate->set_generation(generation);

  return true;
}

net::Socket* P2PTransportImpl::GetChannel() {
  if (pseudo_tcp_adapter_.get()) {
    DCHECK(!channel_adapter_.get());
    return pseudo_tcp_adapter_.get();
  } else {
    DCHECK(channel_adapter_.get());
    return channel_adapter_.get();
  }
}

void P2PTransportImpl::OnTcpConnected(int result) {
  if (result < 0) {
    event_handler_->OnError(result);
    return;
  }
  state_ = static_cast<State>(STATE_READABLE | STATE_WRITABLE);
  event_handler_->OnStateChange(state_);
}
