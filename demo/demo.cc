// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "api/public/mdns_service_listener_factory.h"
#include "api/public/mdns_service_publisher_factory.h"
#include "api/public/message_demuxer.h"
#include "api/public/network_service_manager.h"
#include "api/public/protocol_connection_client.h"
#include "api/public/protocol_connection_client_factory.h"
#include "api/public/protocol_connection_server.h"
#include "api/public/protocol_connection_server_factory.h"
#include "api/public/service_listener.h"
#include "api/public/service_publisher.h"
#include "msgs/osp_messages.h"
#include "platform/api/logging.h"
#include "platform/api/network_interface.h"
#include "third_party/tinycbor/src/src/cbor.h"

namespace openscreen {
namespace {

bool g_done = false;
bool g_dump_services = false;

void sigusr1_dump_services(int) {
  g_dump_services = true;
}

void sigint_stop(int) {
  OSP_LOG_INFO << "caught SIGINT, exiting...";
  g_done = true;
}

void SignalThings() {
  struct sigaction usr1_sa;
  struct sigaction int_sa;
  struct sigaction unused;

  usr1_sa.sa_handler = &sigusr1_dump_services;
  sigemptyset(&usr1_sa.sa_mask);
  usr1_sa.sa_flags = 0;

  int_sa.sa_handler = &sigint_stop;
  sigemptyset(&int_sa.sa_mask);
  int_sa.sa_flags = 0;

  sigaction(SIGUSR1, &usr1_sa, &unused);
  sigaction(SIGINT, &int_sa, &unused);

  OSP_LOG_INFO << "signal handlers setup";
  OSP_LOG_INFO << "pid: " << getpid();
}

class AutoMessage final
    : public ProtocolConnectionClient::ConnectionRequestCallback {
 public:
  ~AutoMessage() override = default;

  void TakeRequest(ProtocolConnectionClient::ConnectRequest&& request) {
    request_ = std::move(request);
  }

  void OnConnectionOpened(
      uint64_t request_id,
      std::unique_ptr<ProtocolConnection>&& connection) override {
    request_ = ProtocolConnectionClient::ConnectRequest();
    msgs::CborEncodeBuffer buffer;
    msgs::PresentationConnectionMessage message;
    message.connection_id = 0;
    message.presentation_id = "presentation-id-foo";
    message.message.which = decltype(message.message.which)::kString;
    new (&message.message.str) std::string("message from client");
    if (msgs::EncodePresentationConnectionMessage(message, &buffer))
      connection->Write(buffer.data(), buffer.size());
    connection->CloseWriteEnd();
  }

  void OnConnectionFailed(uint64_t request_id) override {
    request_ = ProtocolConnectionClient::ConnectRequest();
  }

 private:
  ProtocolConnectionClient::ConnectRequest request_;
};

class ListenerObserver final : public ServiceListener::Observer {
 public:
  ~ListenerObserver() override = default;
  void OnStarted() override { OSP_LOG_INFO << "listener started!"; }
  void OnStopped() override { OSP_LOG_INFO << "listener stopped!"; }
  void OnSuspended() override { OSP_LOG_INFO << "listener suspended!"; }
  void OnSearching() override { OSP_LOG_INFO << "listener searching!"; }

  void OnReceiverAdded(const ServiceInfo& info) override {
    OSP_LOG_INFO << "found! " << info.friendly_name;
    if (!auto_message_) {
      auto_message_ = std::make_unique<AutoMessage>();
      auto_message_->TakeRequest(
          NetworkServiceManager::Get()->GetProtocolConnectionClient()->Connect(
              info.v4_endpoint, auto_message_.get()));
    }
  }
  void OnReceiverChanged(const ServiceInfo& info) override {
    OSP_LOG_INFO << "changed! " << info.friendly_name;
  }
  void OnReceiverRemoved(const ServiceInfo& info) override {
    OSP_LOG_INFO << "removed! " << info.friendly_name;
  }
  void OnAllReceiversRemoved() override { OSP_LOG_INFO << "all removed!"; }
  void OnError(ServiceListenerError) override {}
  void OnMetrics(ServiceListener::Metrics) override {}

 private:
  std::unique_ptr<AutoMessage> auto_message_;
};

class PublisherObserver final : public ServicePublisher::Observer {
 public:
  ~PublisherObserver() override = default;

  void OnStarted() override { OSP_LOG_INFO << "publisher started!"; }
  void OnStopped() override { OSP_LOG_INFO << "publisher stopped!"; }
  void OnSuspended() override { OSP_LOG_INFO << "publisher suspended!"; }

  void OnError(ServicePublisherError) override {}
  void OnMetrics(ServicePublisher::Metrics) override {}
};

class ConnectionClientObserver final
    : public ProtocolConnectionServiceObserver {
 public:
  ~ConnectionClientObserver() override = default;
  void OnRunning() override {}
  void OnStopped() override {}

  void OnMetrics(const NetworkMetrics& metrics) override {}
  void OnError(const Error& error) override {}
};

class ConnectionServerObserver final
    : public ProtocolConnectionServer::Observer {
 public:
  class ConnectionObserver final : public ProtocolConnection::Observer {
   public:
    explicit ConnectionObserver(ConnectionServerObserver* parent)
        : parent_(parent) {}
    ~ConnectionObserver() override = default;

    void OnConnectionClosed(const ProtocolConnection& connection) override {
      auto& connections = parent_->connections_;
      connections.erase(
          std::remove_if(
              connections.begin(), connections.end(),
              [this](const std::pair<std::unique_ptr<ConnectionObserver>,
                                     std::unique_ptr<ProtocolConnection>>& p) {
                return p.first.get() == this;
              }),
          connections.end());
    }

   private:
    ConnectionServerObserver* const parent_;
  };

  ~ConnectionServerObserver() override = default;

  void OnRunning() override {}
  void OnStopped() override {}
  void OnSuspended() override {}

  void OnMetrics(const NetworkMetrics& metrics) override {}
  void OnError(const Error& error) override {}

  void OnIncomingConnection(
      std::unique_ptr<ProtocolConnection>&& connection) override {
    auto observer = std::make_unique<ConnectionObserver>(this);
    connection->SetObserver(observer.get());
    connections_.emplace_back(std::move(observer), std::move(connection));
    connections_.back().second->CloseWriteEnd();
  }

 private:
  std::vector<std::pair<std::unique_ptr<ConnectionObserver>,
                        std::unique_ptr<ProtocolConnection>>>
      connections_;
};

class ConnectionMessageCallback final : public MessageDemuxer::MessageCallback {
 public:
  ~ConnectionMessageCallback() override = default;

  ErrorOr<size_t> OnStreamMessage(uint64_t endpoint_id,
                                  uint64_t connection_id,
                                  msgs::Type message_type,
                                  const uint8_t* buffer,
                                  size_t buffer_size,
                                  platform::TimeDelta now) override {
    msgs::PresentationConnectionMessage message;
    ssize_t result = msgs::DecodePresentationConnectionMessage(
        buffer, buffer_size, &message);
    if (result < 0) {
      // TODO(btolsch): Need something better than including tinycbor.
      if (result == -CborErrorUnexpectedEOF) {
        return Error::Code::kCborIncompleteMessage;
      } else {
        return Error::Code::kCborParsing;
      }
    } else {
      OSP_LOG_INFO << "message: " << message.message.str;
      return result;
    }
  }
};

void ListenerDemo() {
  SignalThings();

  ListenerObserver listener_observer;
  MdnsServiceListenerConfig listener_config;
  auto mdns_listener =
      MdnsServiceListenerFactory::Create(listener_config, &listener_observer);

  MessageDemuxer demuxer;
  ConnectionClientObserver client_observer;
  auto connection_client =
      ProtocolConnectionClientFactory::Create(&demuxer, &client_observer);

  auto* network_service = NetworkServiceManager::Create(
      std::move(mdns_listener), nullptr, std::move(connection_client), nullptr);

  network_service->GetMdnsServiceListener()->Start();
  network_service->GetProtocolConnectionClient()->Start();

  while (!g_done) {
    network_service->RunEventLoopOnce();
  }

  network_service->GetMdnsServiceListener()->Stop();
  network_service->GetProtocolConnectionClient()->Stop();

  NetworkServiceManager::Dispose();
}

void PublisherDemo(const std::string& friendly_name) {
  SignalThings();

  PublisherObserver publisher_observer;
  // TODO(btolsch): aggregate initialization probably better?
  ServicePublisher::Config publisher_config;
  publisher_config.friendly_name = friendly_name;
  publisher_config.hostname = "turtle-deadbeef";
  publisher_config.service_instance_name = "deadbeef";
  publisher_config.connection_server_port = 6667;
  auto mdns_publisher = MdnsServicePublisherFactory::Create(
      publisher_config, &publisher_observer);

  ServerConfig server_config;
  std::vector<platform::InterfaceAddresses> interfaces =
      platform::GetInterfaceAddresses();
  for (const auto& interface : interfaces) {
    server_config.connection_endpoints.push_back(
        IPEndpoint{interface.addresses[0].address, 6667});
  }
  MessageDemuxer demuxer;
  ConnectionMessageCallback message_callback;
  MessageDemuxer::MessageWatch message_watch = demuxer.WatchMessageType(
      0, msgs::Type::kPresentationConnectionMessage, &message_callback);
  ConnectionServerObserver server_observer;
  auto connection_server = ProtocolConnectionServerFactory::Create(
      server_config, &demuxer, &server_observer);
  auto* network_service =
      NetworkServiceManager::Create(nullptr, std::move(mdns_publisher), nullptr,
                                    std::move(connection_server));

  network_service->GetMdnsServicePublisher()->Start();
  network_service->GetProtocolConnectionServer()->Start();

  while (!g_done) {
    network_service->RunEventLoopOnce();
  }

  network_service->GetMdnsServicePublisher()->Stop();
  network_service->GetProtocolConnectionServer()->Stop();

  NetworkServiceManager::Dispose();
}

}  // namespace
}  // namespace openscreen

int main(int argc, char** argv) {
  openscreen::platform::LogInit(nullptr);
  openscreen::platform::SetLogLevel(openscreen::platform::LogLevel::kVerbose,
                                    1);
  if (argc == 1) {
    openscreen::ListenerDemo();
  } else {
    openscreen::PublisherDemo(argv[1]);
  }

  return 0;
}
