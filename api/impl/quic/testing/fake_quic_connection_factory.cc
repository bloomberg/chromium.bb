// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/quic/testing/fake_quic_connection_factory.h"

#include <algorithm>

#include "base/make_unique.h"
#include "platform/api/logging.h"

namespace openscreen {

FakeQuicConnectionFactory::FakeQuicConnectionFactory(
    const IPEndpoint& local_endpoint,
    MessageDemuxer* remote_demuxer)
    : remote_demuxer_(remote_demuxer), local_endpoint_(local_endpoint) {}

FakeQuicConnectionFactory::~FakeQuicConnectionFactory() = default;

void FakeQuicConnectionFactory::StartServerConnection(
    const IPEndpoint& endpoint) {
  QuicConnection::Delegate* delegate =
      server_delegate_->NextConnectionDelegate(endpoint);
  auto connection =
      MakeUnique<FakeQuicConnection>(this, next_connection_id_++, delegate);
  pending_connections_.emplace(endpoint, connection.get());
  server_delegate_->OnIncomingConnection(std::move(connection));
}

FakeQuicStream* FakeQuicConnectionFactory::StartIncomingStream(
    const IPEndpoint& endpoint) {
  auto connection_entry = connections_.find(endpoint);
  if (connection_entry == connections_.end())
    return nullptr;
  std::unique_ptr<FakeQuicStream> stream =
      connection_entry->second->MakeIncomingStream();
  FakeQuicStream* ptr = stream.get();
  connection_entry->second->delegate()->OnIncomingStream(
      connection_entry->second->id(), std::move(stream));
  return ptr;
}

FakeQuicStream* FakeQuicConnectionFactory::GetIncomingStream(
    const IPEndpoint& endpoint,
    uint64_t connection_id) {
  auto connection_entry = connections_.find(endpoint);
  if (connection_entry == connections_.end())
    return nullptr;
  auto stream_entry = connection_entry->second->streams().find(connection_id);
  if (stream_entry == connection_entry->second->streams().end())
    return nullptr;
  return stream_entry->second;
}

void FakeQuicConnectionFactory::OnConnectionClosed(QuicConnection* connection) {
  for (auto entry = connections_.begin(); entry != connections_.end();
       ++entry) {
    if (entry->second == connection) {
      connections_.erase(entry);
      return;
    }
  }
  OSP_DCHECK(false) << "reporting an unknown connection as closed";
}

void FakeQuicConnectionFactory::SetServerDelegate(
    ServerDelegate* delegate,
    const std::vector<IPEndpoint>& endpoints) {
  server_delegate_ = delegate;
}

void FakeQuicConnectionFactory::RunTasks() {
  idle_ = true;
  for (auto& connection : connections_) {
    for (auto& stream : connection.second->streams()) {
      std::vector<uint8_t> received_data = stream.second->TakeReceivedData();
      std::vector<uint8_t> written_data = stream.second->TakeWrittenData();
      if (received_data.size()) {
        idle_ = false;
        stream.second->delegate()->OnReceived(
            stream.second, reinterpret_cast<const char*>(received_data.data()),
            received_data.size());
      }
      if (written_data.size()) {
        idle_ = false;
        remote_demuxer_->OnStreamData(0, stream.second->id(),
                                      written_data.data(), written_data.size());
      }
    }
  }
  for (auto& connection : pending_connections_) {
    idle_ = false;
    connection.second->delegate()->OnCryptoHandshakeComplete(
        connection.second->id());
    connections_.emplace(connection.first, connection.second);
  }
  pending_connections_.clear();
}

std::unique_ptr<QuicConnection> FakeQuicConnectionFactory::Connect(
    const IPEndpoint& endpoint,
    QuicConnection::Delegate* connection_delegate) {
  OSP_DCHECK(pending_connections_.find(endpoint) == pending_connections_.end());
  auto connection = MakeUnique<FakeQuicConnection>(this, next_connection_id_++,
                                                   connection_delegate);
  pending_connections_.emplace(endpoint, connection.get());
  return connection;
}

}  // namespace openscreen
