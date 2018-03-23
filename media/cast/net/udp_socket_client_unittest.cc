// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/udp_socket_client.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/test/utility/net_utility.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/ip_endpoint.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;

namespace media {
namespace cast {

namespace {

class MockUdpSocket final : public network::mojom::UDPSocket {
 public:
  MockUdpSocket(network::mojom::UDPSocketRequest request,
                network::mojom::UDPSocketReceiverPtr receiver)
      : binding_(this, std::move(request)), receiver_(std::move(receiver)) {}

  ~MockUdpSocket() override {}

  MOCK_METHOD0(OnSend, void());

  // network::mojom::UDPSocket implementation.
  void Connect(const net::IPEndPoint& remote_addr,
               network::mojom::UDPSocketOptionsPtr options,
               ConnectCallback callback) override {
    std::move(callback).Run(net::OK, test::GetFreeLocalPort());
  }

  void Bind(const net::IPEndPoint& local_addr,
            network::mojom::UDPSocketOptionsPtr options,
            BindCallback callback) override {}
  void SetBroadcast(bool broadcast, SetBroadcastCallback callback) override {}
  void JoinGroup(const net::IPAddress& group_address,
                 JoinGroupCallback callback) override {}
  void LeaveGroup(const net::IPAddress& group_address,
                  LeaveGroupCallback callback) override {}

  void ReceiveMore(uint32_t num_additional_datagrams) override {
    num_ask_for_receive_ += num_additional_datagrams;
  }

  void ReceiveMoreWithBufferSize(uint32_t num_additional_datagrams,
                                 uint32_t buffer_size) override {}
  void SendTo(const net::IPEndPoint& dest_addr,
              base::span<const uint8_t> data,
              const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
              SendToCallback callback) override {}

  void Send(base::span<const uint8_t> data,
            const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
            SendCallback callback) override {
    sending_packet_ = std::make_unique<Packet>(data.begin(), data.end());
    std::move(callback).Run(net::OK);
    OnSend();
  }

  void Close() override {}

  // Simulate receiving a packet from the network.
  void OnReceivedPacket(const Packet& packet) {
    if (num_ask_for_receive_) {
      receiver_->OnReceived(
          net::OK, base::nullopt,
          base::span<const uint8_t>(
              reinterpret_cast<const uint8_t*>(packet.data()), packet.size()));
      ASSERT_LT(0, num_ask_for_receive_);
      --num_ask_for_receive_;
    }
  }

  void VerifySendingPacket(const Packet& packet) {
    EXPECT_TRUE(
        std::equal(packet.begin(), packet.end(), sending_packet_->begin()));
  }

 private:
  mojo::Binding<network::mojom::UDPSocket> binding_;
  network::mojom::UDPSocketReceiverPtr receiver_;
  std::unique_ptr<Packet> sending_packet_;
  int num_ask_for_receive_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MockUdpSocket);
};

class MockNetworkContext final : public network::mojom::NetworkContext {
 public:
  explicit MockNetworkContext(network::mojom::NetworkContextRequest request)
      : binding_(this, std::move(request)) {}
  ~MockNetworkContext() override {}

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

  MOCK_METHOD0(OnUDPSocketCreated, void());
  void CreateUDPSocket(network::mojom::UDPSocketRequest request,
                       network::mojom::UDPSocketReceiverPtr receiver) override {
    udp_socket_ = std::make_unique<MockUdpSocket>(std::move(request),
                                                  std::move(receiver));
    OnUDPSocketCreated();
  }
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

  MockUdpSocket* udp_socket() const { return udp_socket_.get(); }
  void CreateWebSocket(network::mojom::WebSocketRequest request,
                       int process_id,
                       int render_frame_id,
                       const url::Origin& origin) override {}

 private:
  mojo::Binding<network::mojom::NetworkContext> binding_;
  std::unique_ptr<MockUdpSocket> udp_socket_;
  DISALLOW_COPY_AND_ASSIGN(MockNetworkContext);
};

}  // namespace

class UdpSocketClientTest : public ::testing::Test {
 public:
  UdpSocketClientTest() {
    network::mojom::NetworkContextPtr network_context_ptr;
    network_context_ = std::make_unique<MockNetworkContext>(
        mojo::MakeRequest(&network_context_ptr));
    udp_transport_client_ = std::make_unique<UdpSocketClient>(
        test::GetFreeLocalPort(), std::move(network_context_ptr),
        base::OnceClosure());
  }

  ~UdpSocketClientTest() override = default;

  MOCK_METHOD0(OnReceivedPacketCall, void());
  bool OnReceivedPacket(std::unique_ptr<Packet> packet) {
    received_packet_ = std::move(packet);
    OnReceivedPacketCall();
    return true;
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<MockNetworkContext> network_context_;
  std::unique_ptr<UdpSocketClient> udp_transport_client_;
  std::unique_ptr<Packet> received_packet_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UdpSocketClientTest);
};

TEST_F(UdpSocketClientTest, SendAndReceive) {
  std::string data = "Test";
  Packet packet(data.begin(), data.end());

  {
    // Expect the UDPSocket to be created when calling StartReceiving().
    base::RunLoop run_loop;
    EXPECT_CALL(*network_context_, OnUDPSocketCreated())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    udp_transport_client_->StartReceiving(base::BindRepeating(
        &UdpSocketClientTest::OnReceivedPacket, base::Unretained(this)));
    run_loop.Run();
  }
  scoped_task_environment_.RunUntilIdle();

  MockUdpSocket* socket = network_context_->udp_socket();

  {
    // Request to send one packet.
    base::RunLoop run_loop;
    base::RepeatingClosure cb;
    EXPECT_CALL(*socket, OnSend())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    EXPECT_TRUE(udp_transport_client_->SendPacket(
        new base::RefCountedData<Packet>(packet), cb));
    run_loop.Run();
  }

  // Expect the packet to be sent is delivered to the UDPSocket.
  socket->VerifySendingPacket(packet);

  // Test receiving packet.
  std::string data2 = "Hello";
  Packet packet2(data.begin(), data.end());
  {
    // Simulate receiving |packet2| from the network.
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnReceivedPacketCall())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    socket->OnReceivedPacket(packet2);
    run_loop.Run();
  }

  // The packet is expected to be received.
  EXPECT_TRUE(
      std::equal(packet2.begin(), packet2.end(), received_packet_->begin()));

  udp_transport_client_->StopReceiving();
}

TEST_F(UdpSocketClientTest, SendBeforeConnected) {
  std::string data = "Test";
  Packet packet(data.begin(), data.end());

  // Request to send one packet.
  base::MockCallback<base::RepeatingClosure> resume_send_cb;
  {
    EXPECT_CALL(resume_send_cb, Run()).Times(0);
    EXPECT_FALSE(udp_transport_client_->SendPacket(
        new base::RefCountedData<Packet>(packet), resume_send_cb.Get()));
    scoped_task_environment_.RunUntilIdle();
  }
  {
    // Expect the UDPSocket to be created when calling StartReceiving().
    base::RunLoop run_loop;
    EXPECT_CALL(*network_context_, OnUDPSocketCreated()).Times(1);
    EXPECT_CALL(resume_send_cb, Run())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    udp_transport_client_->StartReceiving(base::BindRepeating(
        &UdpSocketClientTest::OnReceivedPacket, base::Unretained(this)));
    run_loop.Run();
  }
  udp_transport_client_->StopReceiving();
}

}  // namespace cast
}  // namespace media
