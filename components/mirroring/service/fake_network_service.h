// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_FAKE_NETWORK_SERVICE_H_
#define COMPONENTS_MIRRORING_SERVICE_FAKE_NETWORK_SERVICE_H_

#include "base/callback.h"
#include "media/cast/net/cast_transport_config.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mirroring {

class MockUdpSocket final : public network::mojom::UDPSocket {
 public:
  MockUdpSocket(network::mojom::UDPSocketRequest request,
                network::mojom::UDPSocketReceiverPtr receiver);
  ~MockUdpSocket() override;

  MOCK_METHOD0(OnSend, void());

  // network::mojom::UDPSocket implementation.
  void Connect(const net::IPEndPoint& remote_addr,
               network::mojom::UDPSocketOptionsPtr options,
               ConnectCallback callback) override;
  void Bind(const net::IPEndPoint& local_addr,
            network::mojom::UDPSocketOptionsPtr options,
            BindCallback callback) override {}
  void SetBroadcast(bool broadcast, SetBroadcastCallback callback) override {}
  void JoinGroup(const net::IPAddress& group_address,
                 JoinGroupCallback callback) override {}
  void LeaveGroup(const net::IPAddress& group_address,
                  LeaveGroupCallback callback) override {}
  void ReceiveMore(uint32_t num_additional_datagrams) override;
  void ReceiveMoreWithBufferSize(uint32_t num_additional_datagrams,
                                 uint32_t buffer_size) override {}
  void SendTo(const net::IPEndPoint& dest_addr,
              base::span<const uint8_t> data,
              const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
              SendToCallback callback) override {}
  void Send(base::span<const uint8_t> data,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
            SendCallback callback) override;
  void Close() override {}

  // Simulate receiving a packet from the network.
  void OnReceivedPacket(const media::cast::Packet& packet);

  void VerifySendingPacket(const media::cast::Packet& packet);

 private:
  mojo::Binding<network::mojom::UDPSocket> binding_;
  network::mojom::UDPSocketReceiverPtr receiver_;
  std::unique_ptr<media::cast::Packet> sending_packet_;
  int num_ask_for_receive_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MockUdpSocket);
};

class MockNetworkContext final : public network::mojom::NetworkContext {
 public:
  explicit MockNetworkContext(network::mojom::NetworkContextRequest request);
  ~MockNetworkContext() override;

  MOCK_METHOD0(OnUDPSocketCreated, void());

  // network::mojom::NetworkContext implementation:
  void CreateURLLoaderFactory(network::mojom::URLLoaderFactoryRequest request,
                              uint32_t process_id) override {}
  void GetCookieManager(network::mojom::CookieManagerRequest request) override {
  }
  void GetRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerRequest request,
      int32_t render_process_id,
      int32_t render_frame_id) override {}
  void ClearNetworkingHistorySince(
      base::Time time,
      base::OnceClosure completion_callback) override {}
  void ClearHttpCache(base::Time start_time,
                      base::Time end_time,
                      network::mojom::ClearCacheUrlFilterPtr filter,
                      ClearHttpCacheCallback callback) override {}
  void SetNetworkConditions(
      const std::string& profile_id,
      network::mojom::NetworkConditionsPtr conditions) override {}
  void SetAcceptLanguage(const std::string& new_accept_language) override {}
  void AddHSTSForTesting(const std::string& host,
                         base::Time expiry,
                         bool include_subdomains,
                         AddHSTSForTestingCallback callback) override {}
  void CreateUDPSocket(network::mojom::UDPSocketRequest request,
                       network::mojom::UDPSocketReceiverPtr receiver) override;
  void CreateTCPServerSocket(
      const net::IPEndPoint& local_addr,
      uint32_t backlog,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      network::mojom::TCPServerSocketRequest request,
      CreateTCPServerSocketCallback callback) override {}
  void CreateTCPConnectedSocket(
      const base::Optional<net::IPEndPoint>& local_addr,
      const net::AddressList& remote_addr_list,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      network::mojom::TCPConnectedSocketRequest request,
      network::mojom::TCPConnectedSocketObserverPtr observer,
      CreateTCPConnectedSocketCallback callback) override {}
  void CreateWebSocket(network::mojom::WebSocketRequest request,
                       int process_id,
                       int render_frame_id,
                       const url::Origin& origin) override {}

  MockUdpSocket* udp_socket() const { return udp_socket_.get(); }

 private:
  mojo::Binding<network::mojom::NetworkContext> binding_;
  std::unique_ptr<MockUdpSocket> udp_socket_;
  DISALLOW_COPY_AND_ASSIGN(MockNetworkContext);
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_FAKE_NETWORK_SERVICE_H_
