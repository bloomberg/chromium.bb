// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_SENDER_CHANNEL_SENDER_SOCKET_FACTORY_H_
#define CAST_SENDER_CHANNEL_SENDER_SOCKET_FACTORY_H_

#include <openssl/x509.h>

#include <set>
#include <utility>
#include <vector>

#include "cast/common/channel/cast_socket.h"
#include "cast/sender/channel/cast_auth_util.h"
#include "platform/api/tls_connection_factory.h"
#include "platform/base/ip_address.h"
#include "util/logging.h"

namespace cast {
namespace channel {

class SenderSocketFactory final
    : public openscreen::TlsConnectionFactory::Client,
      public CastSocket::Client {
 public:
  class Client {
   public:
    virtual void OnConnected(SenderSocketFactory* factory,
                             const openscreen::IPEndpoint& endpoint,
                             std::unique_ptr<CastSocket> socket) = 0;
    virtual void OnError(SenderSocketFactory* factory,
                         const openscreen::IPEndpoint& endpoint,
                         openscreen::Error error) = 0;
  };

  enum class DeviceMediaPolicy {
    kAudioOnly,
    kIncludesVideo,
  };

  // |client| must outlive |this|.
  explicit SenderSocketFactory(Client* client);
  ~SenderSocketFactory();

  void set_factory(openscreen::TlsConnectionFactory* factory) {
    OSP_DCHECK(factory);
    factory_ = factory;
  }

  void Connect(const openscreen::IPEndpoint& endpoint,
               DeviceMediaPolicy media_policy,
               CastSocket::Client* client);

  // openscreen::TlsConnectionFactory::Client overrides.
  void OnAccepted(
      openscreen::TlsConnectionFactory* factory,
      std::vector<uint8_t> der_x509_peer_cert,
      std::unique_ptr<openscreen::TlsConnection> connection) override;
  void OnConnected(
      openscreen::TlsConnectionFactory* factory,
      std::vector<uint8_t> der_x509_peer_cert,
      std::unique_ptr<openscreen::TlsConnection> connection) override;
  void OnConnectionFailed(
      openscreen::TlsConnectionFactory* factory,
      const openscreen::IPEndpoint& remote_address) override;
  void OnError(openscreen::TlsConnectionFactory* factory,
               openscreen::Error error) override;

 private:
  struct PendingConnection {
    openscreen::IPEndpoint endpoint;
    DeviceMediaPolicy media_policy;
    CastSocket::Client* client;
  };

  struct PendingAuth {
    openscreen::IPEndpoint endpoint;
    DeviceMediaPolicy media_policy;
    std::unique_ptr<CastSocket> socket;
    CastSocket::Client* client;
    AuthContext auth_context;
    bssl::UniquePtr<X509> peer_cert;
  };

  friend bool operator<(const std::unique_ptr<PendingAuth>& a, uint32_t b);
  friend bool operator<(uint32_t a, const std::unique_ptr<PendingAuth>& b);

  std::vector<PendingConnection>::iterator FindPendingConnection(
      const openscreen::IPEndpoint& endpoint);

  // CastSocket::Client overrides.
  void OnError(CastSocket* socket, openscreen::Error error) override;
  void OnMessage(CastSocket* socket, CastMessage message) override;

  Client* const client_;
  openscreen::TlsConnectionFactory* factory_ = nullptr;
  std::vector<PendingConnection> pending_connections_;
  std::vector<std::unique_ptr<PendingAuth>> pending_auth_;
};

}  // namespace channel
}  // namespace cast

#endif  // CAST_SENDER_CHANNEL_SENDER_SOCKET_FACTORY_H_
