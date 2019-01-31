// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "api/impl/quic/quic_connection_factory_impl.h"

#include <algorithm>
#include <memory>

#include "api/impl/quic/quic_connection_impl.h"
#include "platform/api/logging.h"
#include "platform/base/event_loop.h"
#include "third_party/chromium_quic/src/base/location.h"
#include "third_party/chromium_quic/src/base/task_runner.h"
#include "third_party/chromium_quic/src/net/third_party/quic/core/quic_constants.h"
#include "third_party/chromium_quic/src/net/third_party/quic/platform/impl/quic_chromium_clock.h"

namespace openscreen {

struct Task {
  ::base::Location whence;
  ::base::OnceClosure task;
  ::base::TimeDelta delay;
};

class QuicTaskRunner final : public ::base::TaskRunner {
 public:
  QuicTaskRunner();
  ~QuicTaskRunner() override;

  void RunTasks();

  // base::TaskRunner overrides.
  bool PostDelayedTask(const ::base::Location& whence,
                       ::base::OnceClosure task,
                       ::base::TimeDelta delay) override;

  bool RunsTasksInCurrentSequence() const override;

 private:
  uint64_t last_run_unix_;
  std::list<Task> tasks_;
};

QuicTaskRunner::QuicTaskRunner() = default;
QuicTaskRunner::~QuicTaskRunner() = default;

void QuicTaskRunner::RunTasks() {
  auto* clock = ::quic::QuicChromiumClock::GetInstance();
  ::quic::QuicWallTime now = clock->WallNow();
  uint64_t now_unix = now.ToUNIXMicroseconds();
  for (auto it = tasks_.begin(); it != tasks_.end();) {
    Task& next_task = *it;
    next_task.delay -=
        ::base::TimeDelta::FromMicroseconds(now_unix - last_run_unix_);
    if (next_task.delay.InMicroseconds() < 0) {
      std::move(next_task.task).Run();
      it = tasks_.erase(it);
    } else {
      ++it;
    }
  }
  last_run_unix_ = now_unix;
}

bool QuicTaskRunner::PostDelayedTask(const ::base::Location& whence,
                                     ::base::OnceClosure task,
                                     ::base::TimeDelta delay) {
  tasks_.push_back({whence, std::move(task), delay});
  return true;
}

bool QuicTaskRunner::RunsTasksInCurrentSequence() const {
  return true;
}

QuicConnectionFactoryImpl::QuicConnectionFactoryImpl() {
  task_runner_ = ::base::MakeRefCounted<QuicTaskRunner>();
  alarm_factory_ = std::make_unique<::net::QuicChromiumAlarmFactory>(
      task_runner_.get(), ::quic::QuicChromiumClock::GetInstance());
  ::quic::QuartcFactoryConfig factory_config;
  factory_config.alarm_factory = alarm_factory_.get();
  factory_config.clock = ::quic::QuicChromiumClock::GetInstance();
  quartc_factory_ = std::make_unique<::quic::QuartcFactory>(factory_config);
  waiter_ = platform::CreateEventWaiter();
}

QuicConnectionFactoryImpl::~QuicConnectionFactoryImpl() {
  OSP_DCHECK(connections_.empty());
  for (auto* socket : server_sockets_)
    platform::DestroyUdpSocket(socket);
  platform::DestroyEventWaiter(waiter_);
}

void QuicConnectionFactoryImpl::SetServerDelegate(
    ServerDelegate* delegate,
    const std::vector<IPEndpoint>& endpoints) {
  server_delegate_ = delegate;
  server_sockets_.reserve(endpoints.size());
  for (const auto& endpoint : endpoints) {
    auto server_socket = (endpoint.address.version() == IPAddress::Version::kV4)
                             ? platform::CreateUdpSocketIPv4()
                             : platform::CreateUdpSocketIPv6();
    platform::BindUdpSocket(server_socket, endpoint, 0);
    platform::WatchUdpSocketReadable(waiter_, server_socket);
    server_sockets_.push_back(server_socket);
  }
}

void QuicConnectionFactoryImpl::RunTasks() {
  for (const auto& packet : platform::OnePlatformLoopIteration(waiter_)) {
    // TODO(btolsch): We will need to rethink this both for ICE and connection
    // migration support.
    auto conn_it = connections_.find(packet.source);
    if (conn_it == connections_.end()) {
      if (server_delegate_) {
        OSP_VLOG(1) << __func__ << ": spawning connection from "
                    << packet.source;
        auto transport =
            std::make_unique<UdpTransport>(packet.socket, packet.source);
        ::quic::QuartcSessionConfig session_config;
        session_config.perspective = ::quic::Perspective::IS_SERVER;
        session_config.packet_transport = transport.get();

        auto result = std::make_unique<QuicConnectionImpl>(
            this, server_delegate_->NextConnectionDelegate(packet.source),
            std::move(transport),
            quartc_factory_->CreateQuartcSession(session_config));
        connections_.emplace(packet.source, result.get());
        auto* result_ptr = result.get();
        server_delegate_->OnIncomingConnection(std::move(result));
        result_ptr->OnDataReceived(packet);
      }
    } else {
      OSP_VLOG(1) << __func__ << ": data for existing connection from "
                  << packet.source;
      conn_it->second->OnDataReceived(packet);
    }
  }
}

std::unique_ptr<QuicConnection> QuicConnectionFactoryImpl::Connect(
    const IPEndpoint& endpoint,
    QuicConnection::Delegate* connection_delegate) {
  auto* socket = endpoint.address.IsV4() ? platform::CreateUdpSocketIPv4()
                                         : platform::CreateUdpSocketIPv6();
  platform::BindUdpSocket(socket, {}, 0);
  auto transport = std::make_unique<UdpTransport>(socket, endpoint);

  ::quic::QuartcSessionConfig session_config;
  session_config.perspective = ::quic::Perspective::IS_CLIENT;
  // TODO(btolsch): Proper server id.  Does this go in the QUIC server name
  // parameter?
  session_config.unique_remote_server_id = "turtle";
  session_config.packet_transport = transport.get();

  auto result = std::make_unique<QuicConnectionImpl>(
      this, connection_delegate, std::move(transport),
      quartc_factory_->CreateQuartcSession(session_config));

  platform::WatchUdpSocketReadable(waiter_, socket);
  // TODO(btolsch): This presents a problem for multihomed receivers, which may
  // register as a different endpoint in their response.  I think QUIC is
  // already tolerant of this via connection IDs but this hasn't been tested
  // (and even so, those aren't necessarily stable either).
  connections_.emplace(endpoint, result.get());

  return result;
}

void QuicConnectionFactoryImpl::OnConnectionClosed(QuicConnection* connection) {
  auto entry = std::find_if(
      connections_.begin(), connections_.end(),
      [connection](const std::pair<IPEndpoint, QuicConnection*>& entry) {
        return entry.second == connection;
      });
  connections_.erase(entry);
}

}  // namespace openscreen
