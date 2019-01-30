// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/quic/testing/quic_test_support.h"

#include <memory>

#include "api/impl/quic/quic_client.h"
#include "api/impl/quic/quic_server.h"
#include "api/public/network_service_manager.h"

namespace openscreen {

FakeQuicBridge::FakeQuicBridge() {
  fake_bridge_ =
      std::make_unique<FakeQuicConnectionFactoryBridge>(kControllerEndpoint);
  platform::TimeDelta now = platform::TimeDelta::FromMilliseconds(1298424);

  fake_clock_ = std::make_unique<FakeClock>(now);

  controller_demuxer_ = std::make_unique<MessageDemuxer>(
      MessageDemuxer::kDefaultBufferLimit,
      std::make_unique<FakeClock>(*fake_clock_));
  receiver_demuxer_ = std::make_unique<MessageDemuxer>(
      MessageDemuxer::kDefaultBufferLimit,
      std::make_unique<FakeClock>(*fake_clock_));

  auto fake_client_factory =
      std::make_unique<FakeClientQuicConnectionFactory>(fake_bridge_.get());
  auto quic_client = std::make_unique<QuicClient>(
      controller_demuxer_.get(), std::move(fake_client_factory),
      &mock_client_observer_);

  auto fake_server_factory =
      std::make_unique<FakeServerQuicConnectionFactory>(fake_bridge_.get());
  ServerConfig config;
  config.connection_endpoints.push_back(kReceiverEndpoint);
  auto quic_server = std::make_unique<QuicServer>(
      config, receiver_demuxer_.get(), std::move(fake_server_factory),
      &mock_server_observer_);

  quic_client->Start();
  quic_server->Start();

  NetworkServiceManager::Get()->Create(nullptr, nullptr, std::move(quic_client),
                                       std::move(quic_server));
}

FakeQuicBridge::~FakeQuicBridge() {
  NetworkServiceManager::Get()->Dispose();
}

void FakeQuicBridge::RunTasksUntilIdle() {
  do {
    NetworkServiceManager::Get()->GetProtocolConnectionClient()->RunTasks();
    NetworkServiceManager::Get()->GetProtocolConnectionServer()->RunTasks();
  } while (!fake_bridge_->idle());
}

}  // namespace openscreen
