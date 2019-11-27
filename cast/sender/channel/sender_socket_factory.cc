// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/sender/channel/sender_socket_factory.h"

#include "cast/common/channel/cast_socket.h"
#include "cast/sender/channel/message_util.h"
#include "platform/base/tls_connect_options.h"
#include "util/crypto/certificate_utils.h"

namespace cast {
namespace channel {

using openscreen::platform::TlsConnectOptions;

bool operator<(const std::unique_ptr<SenderSocketFactory::PendingAuth>& a,
               uint32_t b) {
  return a && a->socket->socket_id() < b;
}

bool operator<(uint32_t a,
               const std::unique_ptr<SenderSocketFactory::PendingAuth>& b) {
  return b && a < b->socket->socket_id();
}

SenderSocketFactory::SenderSocketFactory(Client* client) : client_(client) {
  OSP_DCHECK(client);
}

SenderSocketFactory::~SenderSocketFactory() = default;

void SenderSocketFactory::Connect(const IPEndpoint& endpoint,
                                  DeviceMediaPolicy media_policy,
                                  CastSocket::Client* client) {
  OSP_DCHECK(client);
  auto it = FindPendingConnection(endpoint);
  if (it == pending_connections_.end()) {
    pending_connections_.emplace_back(
        PendingConnection{endpoint, media_policy, client});
    factory_->Connect(endpoint, TlsConnectOptions{true});
  }
}

void SenderSocketFactory::OnAccepted(
    TlsConnectionFactory* factory,
    std::vector<uint8_t> der_x509_peer_cert,
    std::unique_ptr<TlsConnection> connection) {
  OSP_NOTREACHED() << "This factory is connect-only.";
}

void SenderSocketFactory::OnConnected(
    TlsConnectionFactory* factory,
    std::vector<uint8_t> der_x509_peer_cert,
    std::unique_ptr<TlsConnection> connection) {
  const IPEndpoint& endpoint = connection->GetRemoteEndpoint();
  auto it = FindPendingConnection(endpoint);
  if (it == pending_connections_.end()) {
    OSP_DLOG_ERROR << "TLS connection succeeded for unknown endpoint: "
                   << endpoint;
    return;
  }
  DeviceMediaPolicy media_policy = it->media_policy;
  CastSocket::Client* client = it->client;
  pending_connections_.erase(it);

  ErrorOr<bssl::UniquePtr<X509>> peer_cert = openscreen::ImportCertificate(
      der_x509_peer_cert.data(), der_x509_peer_cert.size());
  if (!peer_cert) {
    client_->OnError(this, endpoint, peer_cert.error());
    return;
  }

  auto socket = std::make_unique<CastSocket>(std::move(connection), this,
                                             GetNextSocketId());
  pending_auth_.emplace_back(
      new PendingAuth{endpoint, media_policy, std::move(socket), client,
                      AuthContext::Create(), std::move(peer_cert.value())});
  PendingAuth& pending = *pending_auth_.back();

  CastMessage auth_challenge = CreateAuthChallengeMessage(pending.auth_context);
  Error error = pending.socket->SendMessage(auth_challenge);
  if (!error.ok()) {
    pending_auth_.pop_back();
    client_->OnError(this, endpoint, error);
  }
}

void SenderSocketFactory::OnConnectionFailed(TlsConnectionFactory* factory,
                                             const IPEndpoint& remote_address) {
  auto it = FindPendingConnection(remote_address);
  if (it == pending_connections_.end()) {
    OSP_DVLOG << "OnConnectionFailed reported for untracked address: "
              << remote_address;
    return;
  }
  pending_connections_.erase(it);
  client_->OnError(this, remote_address, Error::Code::kConnectionFailed);
}

void SenderSocketFactory::OnError(TlsConnectionFactory* factory, Error error) {
  std::vector<PendingConnection> connections;
  pending_connections_.swap(connections);
  for (const PendingConnection& pending : connections) {
    client_->OnError(this, pending.endpoint, error);
  }
}

std::vector<SenderSocketFactory::PendingConnection>::iterator
SenderSocketFactory::FindPendingConnection(const IPEndpoint& endpoint) {
  return std::find_if(pending_connections_.begin(), pending_connections_.end(),
                      [&endpoint](const PendingConnection& pending) {
                        return pending.endpoint == endpoint;
                      });
}

void SenderSocketFactory::OnError(CastSocket* socket, Error error) {
  auto it = std::find_if(pending_auth_.begin(), pending_auth_.end(),
                         [id = socket->socket_id()](
                             const std::unique_ptr<PendingAuth>& pending_auth) {
                           return pending_auth->socket->socket_id() == id;
                         });
  if (it == pending_auth_.end()) {
    OSP_DLOG_ERROR << "Got error for unknown pending socket";
    return;
  }
  IPEndpoint endpoint = (*it)->endpoint;
  pending_auth_.erase(it);
  client_->OnError(this, endpoint, error);
}

void SenderSocketFactory::OnMessage(CastSocket* socket, CastMessage message) {
  auto it = std::find_if(pending_auth_.begin(), pending_auth_.end(),
                         [id = socket->socket_id()](
                             const std::unique_ptr<PendingAuth>& pending_auth) {
                           return pending_auth->socket->socket_id() == id;
                         });
  if (it == pending_auth_.end()) {
    OSP_DLOG_ERROR << "Got message for unknown pending socket";
    return;
  }

  std::unique_ptr<PendingAuth> pending = std::move(*it);
  pending_auth_.erase(it);
  if (!IsAuthMessage(message)) {
    client_->OnError(this, pending->endpoint,
                     Error::Code::kCastV2AuthenticationError);
    return;
  }

  ErrorOr<CastDeviceCertPolicy> policy_or_error = AuthenticateChallengeReply(
      message, (*it)->peer_cert.get(), (*it)->auth_context);
  if (policy_or_error.is_error()) {
    client_->OnError(this, pending->endpoint, policy_or_error.error());
    return;
  }

  if (policy_or_error.value() == CastDeviceCertPolicy::kAudioOnly &&
      pending->media_policy != DeviceMediaPolicy::kAudioOnly) {
    client_->OnError(this, pending->endpoint,
                     Error::Code::kCastV2ChannelPolicyMismatch);
    return;
  }

  pending->socket->SetClient(pending->client);
  client_->OnConnected(this, pending->endpoint, std::move(pending->socket));
}

}  // namespace channel
}  // namespace cast
