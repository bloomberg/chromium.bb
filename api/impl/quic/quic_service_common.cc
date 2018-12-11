// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/quic/quic_service_common.h"

#include "base/make_unique.h"
#include "platform/api/logging.h"

namespace openscreen {

// static
std::unique_ptr<QuicProtocolConnection> QuicProtocolConnection::FromExisting(
    Owner* owner,
    QuicConnection* connection,
    ServiceConnectionDelegate* delegate,
    uint64_t endpoint_id) {
  std::unique_ptr<QuicStream> stream = connection->MakeOutgoingStream(delegate);
  auto pc =
      MakeUnique<QuicProtocolConnection>(owner, endpoint_id, stream->id());
  pc->set_stream(stream.get());
  delegate->AddStreamPair(ServiceStreamPair(std::move(stream), pc.get()));
  return pc;
}

QuicProtocolConnection::QuicProtocolConnection(Owner* owner,
                                               uint64_t endpoint_id,
                                               uint64_t connection_id)
    : ProtocolConnection(endpoint_id, connection_id), owner_(owner) {}

QuicProtocolConnection::~QuicProtocolConnection() {
  if (stream_)
    owner_->OnConnectionDestroyed(this);
}

void QuicProtocolConnection::Write(const uint8_t* data, size_t data_size) {
  if (stream_)
    stream_->Write(data, data_size);
}

void QuicProtocolConnection::CloseWriteEnd() {
  if (stream_)
    stream_->CloseWriteEnd();
}

void QuicProtocolConnection::OnClose() {
  if (observer_)
    observer_->OnConnectionClosed(*this);
}

ServiceStreamPair::ServiceStreamPair(
    std::unique_ptr<QuicStream>&& stream,
    QuicProtocolConnection* protocol_connection)
    : stream(std::move(stream)),
      protocol_connection(std::move(protocol_connection)) {}
ServiceStreamPair::~ServiceStreamPair() = default;

ServiceStreamPair::ServiceStreamPair(ServiceStreamPair&& other) = default;

ServiceStreamPair& ServiceStreamPair::operator=(ServiceStreamPair&& other) =
    default;

ServiceConnectionDelegate::ServiceConnectionDelegate(ServiceDelegate* parent,
                                                     const IPEndpoint& endpoint)
    : parent_(parent), endpoint_(endpoint) {}

ServiceConnectionDelegate::~ServiceConnectionDelegate() {
  OSP_DCHECK(streams_.empty());
}

void ServiceConnectionDelegate::AddStreamPair(ServiceStreamPair&& stream_pair) {
  uint64_t stream_id = stream_pair.stream->id();
  streams_.emplace(stream_id, std::move(stream_pair));
}

void ServiceConnectionDelegate::DropProtocolConnection(
    QuicProtocolConnection* connection) {
  auto stream_entry = streams_.find(connection->stream()->id());
  if (stream_entry == streams_.end())
    return;
  stream_entry->second.protocol_connection = nullptr;
}

void ServiceConnectionDelegate::OnCryptoHandshakeComplete(
    uint64_t connection_id) {
  endpoint_id_ = parent_->OnCryptoHandshakeComplete(this, connection_id);
}

void ServiceConnectionDelegate::OnIncomingStream(
    uint64_t connection_id,
    std::unique_ptr<QuicStream> stream) {
  pending_connection_->set_stream(stream.get());
  AddStreamPair(
      ServiceStreamPair(std::move(stream), pending_connection_.get()));
  parent_->OnIncomingStream(std::move(pending_connection_));
}

void ServiceConnectionDelegate::OnConnectionClosed(uint64_t connection_id) {
  parent_->OnConnectionClosed(endpoint_id_, connection_id);
}

QuicStream::Delegate* ServiceConnectionDelegate::NextStreamDelegate(
    uint64_t connection_id,
    uint64_t stream_id) {
  OSP_DCHECK(!pending_connection_);
  pending_connection_ =
      MakeUnique<QuicProtocolConnection>(parent_, endpoint_id_, stream_id);
  return this;
}

void ServiceConnectionDelegate::OnReceived(QuicStream* stream,
                                           const char* data,
                                           size_t data_size) {
  auto stream_entry = streams_.find(stream->id());
  if (stream_entry == streams_.end())
    return;
  // TODO(btolsch): It happens that for normal stream data, OnClose is called
  // before the fin bit is passed here.  Would OnClose instead be the last
  // callback if a RST_STREAM or CONNECTION_CLOSE is received?
  if (stream_entry->second.protocol_connection) {
    parent_->OnDataReceived(
        stream_entry->second.protocol_connection->endpoint_id(),
        stream_entry->second.protocol_connection->connection_id(),
        reinterpret_cast<const uint8_t*>(data), data_size);
  }
  if (!data_size) {
    if (stream_entry->second.protocol_connection)
      stream_entry->second.protocol_connection->set_stream(nullptr);
    streams_.erase(stream_entry);
  }
}

void ServiceConnectionDelegate::OnClose(uint64_t stream_id) {
  auto stream_entry = streams_.find(stream_id);
  if (stream_entry == streams_.end())
    return;
  if (stream_entry->second.protocol_connection)
    stream_entry->second.protocol_connection->OnClose();
}

ServiceConnectionData::ServiceConnectionData(
    std::unique_ptr<QuicConnection>&& connection,
    std::unique_ptr<ServiceConnectionDelegate>&& delegate)
    : connection(std::move(connection)), delegate(std::move(delegate)) {}
ServiceConnectionData::ServiceConnectionData(ServiceConnectionData&&) = default;
ServiceConnectionData::~ServiceConnectionData() = default;
ServiceConnectionData& ServiceConnectionData::operator=(
    ServiceConnectionData&&) = default;

}  // namespace openscreen
