// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef API_IMPL_QUIC_TESTING_FAKE_QUIC_CONNECTION_FACTORY_H_
#define API_IMPL_QUIC_TESTING_FAKE_QUIC_CONNECTION_FACTORY_H_

#include <vector>

#include "api/impl/quic/quic_connection_factory.h"
#include "api/impl/quic/testing/fake_quic_connection.h"
#include "api/public/message_demuxer.h"

namespace openscreen {

class FakeQuicConnectionFactory final : public QuicConnectionFactory {
 public:
  FakeQuicConnectionFactory(const IPEndpoint& local_endpoint,
                            MessageDemuxer* remote_demuxer);
  ~FakeQuicConnectionFactory() override;

  bool idle() const { return idle_; }

  void StartServerConnection(const IPEndpoint& endpoint);
  FakeQuicStream* StartIncomingStream(const IPEndpoint& endpoint);
  FakeQuicStream* GetIncomingStream(const IPEndpoint& endpoint,
                                    uint64_t connection_id);
  void OnConnectionClosed(QuicConnection* connection);

  // QuicConnectionFactory overrides.
  void SetServerDelegate(ServerDelegate* delegate,
                         const std::vector<IPEndpoint>& endpoints) override;
  void RunTasks() override;
  std::unique_ptr<QuicConnection> Connect(
      const IPEndpoint& endpoint,
      QuicConnection::Delegate* connection_delegate) override;

 private:
  ServerDelegate* server_delegate_ = nullptr;
  MessageDemuxer* const remote_demuxer_;
  const IPEndpoint local_endpoint_;
  bool idle_ = true;
  uint64_t next_connection_id_ = 0;
  std::map<IPEndpoint, FakeQuicConnection*, IPEndpointComparator>
      pending_connections_;
  std::map<IPEndpoint, FakeQuicConnection*, IPEndpointComparator> connections_;
};

}  // namespace openscreen

#endif  // API_IMPL_QUIC_TESTING_FAKE_QUIC_CONNECTION_FACTORY_H_
