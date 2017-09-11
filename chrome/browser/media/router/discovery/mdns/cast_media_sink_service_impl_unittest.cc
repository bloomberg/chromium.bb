// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/media/router/test_helper.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithArgs;
using ::testing::_;
using cast_channel::ChannelError;

namespace {

net::IPEndPoint CreateIPEndPoint(int num) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(
      base::StringPrintf("192.168.0.10%d", num)));
  return net::IPEndPoint(ip_address, 8009 + num);
}

media_router::MediaSinkInternal CreateCastSink(int num) {
  std::string friendly_name = base::StringPrintf("friendly name %d", num);
  std::string unique_id = base::StringPrintf("id %d", num);
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(num);

  media_router::MediaSink sink(unique_id, friendly_name,
                               media_router::SinkIconType::CAST);
  media_router::CastSinkExtraData extra_data;
  extra_data.ip_endpoint = ip_endpoint;
  extra_data.port = ip_endpoint.port();
  extra_data.model_name = base::StringPrintf("model name %d", num);
  extra_data.cast_channel_id = num;
  extra_data.capabilities = cast_channel::CastDeviceCapability::AUDIO_OUT |
                            cast_channel::CastDeviceCapability::VIDEO_OUT;
  return media_router::MediaSinkInternal(sink, extra_data);
}

media_router::MediaSinkInternal CreateDialSink(int num) {
  std::string friendly_name = base::StringPrintf("friendly name %d", num);
  std::string unique_id = base::StringPrintf("id %d", num);
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(num);

  media_router::MediaSink sink(unique_id, friendly_name,
                               media_router::SinkIconType::GENERIC);
  media_router::DialSinkExtraData extra_data;
  extra_data.ip_address = ip_endpoint.address();
  extra_data.model_name = base::StringPrintf("model name %d", num);
  return media_router::MediaSinkInternal(sink, extra_data);
}

}  // namespace

namespace media_router {

class CastMediaSinkServiceImplTest : public ::testing::Test {
 public:
  CastMediaSinkServiceImplTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        mock_cast_socket_service_(new cast_channel::MockCastSocketService()),
        mock_time_task_runner_(new base::TestMockTimeTaskRunner()),
        media_sink_service_impl_(mock_sink_discovered_cb_.Get(),
                                 mock_cast_socket_service_.get(),
                                 discovery_network_monitor_.get(),
                                 mock_time_task_runner_.get()) {}

  void SetUp() override {
    fake_network_info_.clear();
    auto mock_timer = base::MakeUnique<base::MockTimer>(
        true /*retain_user_task*/, false /*is_repeating*/);
    mock_timer_ = mock_timer.get();
    media_sink_service_impl_.SetTimerForTest(std::move(mock_timer));
  }

 protected:
  void ExpectOpenSocketInternal(cast_channel::CastSocket* socket) {
    EXPECT_CALL(*mock_cast_socket_service_,
                OpenSocketInternal(socket->ip_endpoint(), _, _, _))
        .WillOnce(Invoke([socket](const auto& ip_endpoint, auto* net_log,
                                  auto open_cb, auto* observer) {
          std::move(open_cb).Run(socket);
          return socket->id();
        }));
  }

  cast_channel::CastSocket::Observer& observer() {
    return static_cast<cast_channel::CastSocket::Observer&>(
        media_sink_service_impl_);
  }

  static std::vector<DiscoveryNetworkInfo> FakeGetNetworkInfo() {
    return fake_network_info_;
  }

  static std::vector<DiscoveryNetworkInfo> fake_network_info_;

  std::vector<DiscoveryNetworkInfo> fake_ethernet_info_ = {
      DiscoveryNetworkInfo{std::string("enp0s2"), std::string("ethernet1")}};
  std::vector<DiscoveryNetworkInfo> fake_wifi_info_ = {
      DiscoveryNetworkInfo{std::string("wlp3s0"), std::string("wifi1")},
      DiscoveryNetworkInfo{std::string("wlp3s1"), std::string("wifi2")}};
  std::vector<DiscoveryNetworkInfo> fake_unknown_info_ = {
      DiscoveryNetworkInfo{std::string("enp0s2"), std::string()}};

  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_ =
      base::WrapUnique(net::NetworkChangeNotifier::CreateMock());

  const content::TestBrowserThreadBundle thread_bundle_;

  std::unique_ptr<DiscoveryNetworkMonitor> discovery_network_monitor_ =
      DiscoveryNetworkMonitor::CreateInstanceForTest(&FakeGetNetworkInfo);

  base::MockCallback<MediaSinkService::OnSinksDiscoveredCallback>
      mock_sink_discovered_cb_;
  std::unique_ptr<cast_channel::MockCastSocketService>
      mock_cast_socket_service_;
  base::MockTimer* mock_timer_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner_;
  CastMediaSinkServiceImpl media_sink_service_impl_;

  DISALLOW_COPY_AND_ASSIGN(CastMediaSinkServiceImplTest);
};

// static
std::vector<DiscoveryNetworkInfo>
    CastMediaSinkServiceImplTest::fake_network_info_;

TEST_F(CastMediaSinkServiceImplTest, TestOnChannelOpenSucceeded) {
  auto cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);

  media_sink_service_impl_.OnChannelOpenSucceeded(cast_sink, &socket);

  // Verify sink content
  EXPECT_CALL(mock_sink_discovered_cb_,
              Run(std::vector<MediaSinkInternal>({cast_sink})));
  media_sink_service_impl_.OnFetchCompleted();
}

TEST_F(CastMediaSinkServiceImplTest, TestMultipleOnChannelOpenSucceeded) {
  auto cast_sink1 = CreateCastSink(1);
  auto cast_sink2 = CreateCastSink(2);
  auto cast_sink3 = CreateCastSink(3);
  auto ip_endpoint1 = CreateIPEndPoint(1);
  auto ip_endpoint2 = CreateIPEndPoint(2);
  auto ip_endpoint3 = CreateIPEndPoint(3);

  cast_channel::MockCastSocket socket2;
  socket2.set_id(2);
  cast_channel::MockCastSocket socket3;
  socket3.set_id(3);

  // Current round of Dns discovery finds service1 and service 2.
  // Fail to open channel 1.
  media_sink_service_impl_.OnChannelOpenSucceeded(cast_sink2, &socket2);
  media_sink_service_impl_.OnChannelOpenSucceeded(cast_sink3, &socket3);

  // Verify sink content
  EXPECT_CALL(mock_sink_discovered_cb_,
              Run(std::vector<MediaSinkInternal>({cast_sink2, cast_sink3})));
  media_sink_service_impl_.OnFetchCompleted();
}

TEST_F(CastMediaSinkServiceImplTest, TestTimer) {
  auto cast_sink1 = CreateCastSink(1);
  auto cast_sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);

  EXPECT_FALSE(mock_timer_->IsRunning());
  media_sink_service_impl_.Start();
  EXPECT_TRUE(mock_timer_->IsRunning());

  // Channel 2 is opened.
  cast_channel::MockCastSocket socket2;
  socket2.set_id(2);

  media_sink_service_impl_.OnChannelOpenSucceeded(cast_sink2, &socket2);

  std::vector<MediaSinkInternal> sinks;
  EXPECT_CALL(mock_sink_discovered_cb_, Run(_)).WillOnce(SaveArg<0>(&sinks));

  // Fire timer.
  mock_timer_->Fire();
  EXPECT_EQ(sinks, std::vector<MediaSinkInternal>({cast_sink2}));

  EXPECT_FALSE(mock_timer_->IsRunning());
  // Channel 1 is opened and timer is restarted.
  cast_channel::MockCastSocket socket1;
  socket1.set_id(1);

  media_sink_service_impl_.OnChannelOpenSucceeded(cast_sink1, &socket1);
  EXPECT_TRUE(mock_timer_->IsRunning());
}

TEST_F(CastMediaSinkServiceImplTest, TestOpenChannelNoRetry) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint);
  socket.SetErrorState(cast_channel::ChannelError::NONE);

  // No pending sink
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint, _, _, _))
      .Times(1);
  media_sink_service_impl_.OpenChannel(ip_endpoint, cast_sink, nullptr);

  // One pending sink, the same as |cast_sink|
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint, _, _, _))
      .Times(0);
  media_sink_service_impl_.OpenChannel(ip_endpoint, cast_sink, nullptr);
}

TEST_F(CastMediaSinkServiceImplTest, TestOpenChannelRetryOnce) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint);
  socket.SetErrorState(cast_channel::ChannelError::NONE);
  socket.SetErrorState(cast_channel::ChannelError::CAST_SOCKET_ERROR);

  media_sink_service_impl_.SetTaskRunnerForTest(mock_time_task_runner_);
  std::unique_ptr<net::BackoffEntry> backoff_entry(
      new net::BackoffEntry(&CastMediaSinkServiceImpl::kBackoffPolicy));
  ExpectOpenSocketInternal(&socket);
  media_sink_service_impl_.OpenChannel(ip_endpoint, cast_sink,
                                       std::move(backoff_entry));

  socket.SetErrorState(cast_channel::ChannelError::NONE);
  ExpectOpenSocketInternal(&socket);
  // Wait for 16 seconds.
  mock_time_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(16));
}

TEST_F(CastMediaSinkServiceImplTest, TestOpenChannelFails) {
  MediaSinkInternal cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint);
  socket.SetErrorState(cast_channel::ChannelError::CAST_SOCKET_ERROR);

  media_sink_service_impl_.SetTaskRunnerForTest(mock_time_task_runner_);

  ExpectOpenSocketInternal(&socket);
  net::BackoffEntry::Policy policy = CastMediaSinkServiceImpl::kBackoffPolicy;
  std::unique_ptr<net::BackoffEntry> backoff_entry(
      new net::BackoffEntry(&policy));
  auto* backoff_entry_ptr = backoff_entry.get();
  media_sink_service_impl_.OpenChannel(ip_endpoint, cast_sink,
                                       std::move(backoff_entry));

  // 1st retry attempt
  ExpectOpenSocketInternal(&socket);
  base::TimeDelta delay = backoff_entry_ptr->GetTimeUntilRelease() +
                          base::TimeDelta::FromSeconds(1);
  mock_time_task_runner_->FastForwardBy(delay);
  // 2nd retry attempt
  ExpectOpenSocketInternal(&socket);
  delay = backoff_entry_ptr->GetTimeUntilRelease() +
          base::TimeDelta::FromSeconds(1);
  mock_time_task_runner_->FastForwardBy(delay);
  // 3rd retry attempt
  ExpectOpenSocketInternal(&socket);
  delay = backoff_entry_ptr->GetTimeUntilRelease() +
          base::TimeDelta::FromSeconds(1);
  mock_time_task_runner_->FastForwardBy(delay);
  // No more retry.
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint, _, _, _))
      .Times(0);
  mock_time_task_runner_->FastForwardBy(delay);
}

TEST_F(CastMediaSinkServiceImplTest, TestMultipleOpenChannels) {
  auto cast_sink1 = CreateCastSink(1);
  auto cast_sink2 = CreateCastSink(2);
  auto cast_sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _, _));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _, _));

  // 1st round finds service 1 & 2.
  std::vector<MediaSinkInternal> sinks1{cast_sink1, cast_sink2};
  media_sink_service_impl_.OpenChannels(sinks1);

  // Channel 2 opened.
  cast_channel::MockCastSocket socket2;
  socket2.set_id(2);
  socket2.SetErrorState(cast_channel::ChannelError::NONE);
  media_sink_service_impl_.OnChannelOpened(cast_sink2, nullptr, &socket2);

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _, _));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint3, _, _, _));

  // 2nd round finds service 2 & 3.
  std::vector<MediaSinkInternal> sinks2{cast_sink2, cast_sink3};
  media_sink_service_impl_.OpenChannels(sinks2);

  // Channel 1 and 3 opened.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket3;
  socket1.set_id(1);
  socket3.set_id(3);
  socket1.SetErrorState(cast_channel::ChannelError::NONE);
  socket3.SetErrorState(cast_channel::ChannelError::NONE);
  media_sink_service_impl_.OnChannelOpened(cast_sink1, nullptr, &socket1);
  media_sink_service_impl_.OnChannelOpened(cast_sink3, nullptr, &socket3);

  EXPECT_CALL(mock_sink_discovered_cb_,
              Run(std::vector<MediaSinkInternal>(
                  {cast_sink1, cast_sink2, cast_sink3})));
  media_sink_service_impl_.OnFetchCompleted();
}

TEST_F(CastMediaSinkServiceImplTest, TestOnChannelOpenFailed) {
  auto cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);

  media_sink_service_impl_.OnChannelOpenSucceeded(cast_sink, &socket);

  EXPECT_EQ(1u, media_sink_service_impl_.current_sinks_map_.size());

  socket.SetIPEndpoint(ip_endpoint1);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1);
  EXPECT_TRUE(media_sink_service_impl_.current_sinks_map_.empty());
}

TEST_F(CastMediaSinkServiceImplTest,
       TestOnChannelErrorMayRetryForConnectingChannel) {
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint1);

  media_sink_service_impl_.SetTaskRunnerForTest(mock_time_task_runner_);

  // No op for CONNECTING cast channel.
  EXPECT_CALL(socket, ready_state())
      .WillOnce(Return(cast_channel::ReadyState::CONNECTING));
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocketInternal(_, _, _, _))
      .Times(0);

  media_sink_service_impl_.OnError(
      socket, cast_channel::ChannelError::CHANNEL_NOT_OPEN);
  mock_time_task_runner_->RunUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, TestOnChannelErrorMayRetryForCastSink) {
  auto cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint1);

  media_sink_service_impl_.SetTaskRunnerForTest(mock_time_task_runner_);

  // There is an existing cast sink in |current_sinks_map_|.
  media_sink_service_impl_.current_sinks_map_[ip_endpoint1.address()] =
      cast_sink;
  EXPECT_CALL(socket, ready_state())
      .WillRepeatedly(Return(cast_channel::ReadyState::CLOSED));
  media_sink_service_impl_.OnError(
      socket, cast_channel::ChannelError::CHANNEL_NOT_OPEN);

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _, _));
  mock_time_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(16));
  EXPECT_TRUE(media_sink_service_impl_.current_sinks_map_.empty());

  base::RunLoop().RunUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, TestOnChannelErrorNoRetryForMissingSink) {
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);
  socket.SetIPEndpoint(ip_endpoint1);
  EXPECT_CALL(socket, ready_state())
      .WillOnce(Return(cast_channel::ReadyState::CLOSED));

  media_sink_service_impl_.SetTaskRunnerForTest(mock_time_task_runner_);

  // There is no existing cast sink.
  media_sink_service_impl_.pending_for_open_ip_endpoints_.clear();
  media_sink_service_impl_.current_sinks_map_.clear();

  media_sink_service_impl_.OnError(
      socket, cast_channel::ChannelError::CHANNEL_NOT_OPEN);

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _, _))
      .Times(0);
  mock_time_task_runner_->RunUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, TestOnDialSinkAdded) {
  MediaSinkInternal dial_sink1 = CreateDialSink(1);
  MediaSinkInternal dial_sink2 = CreateDialSink(2);
  net::IPEndPoint ip_endpoint1(dial_sink1.dial_data().ip_address,
                               CastMediaSinkServiceImpl::kCastControlPort);
  net::IPEndPoint ip_endpoint2(dial_sink2.dial_data().ip_address,
                               CastMediaSinkServiceImpl::kCastControlPort);

  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;
  socket1.set_id(1);
  socket2.set_id(2);

  // Channel 1, 2 opened.
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _, _))
      .WillOnce(DoAll(
          WithArgs<2>(Invoke(
              [&](const base::Callback<void(cast_channel::CastSocket * socket)>&
                      callback) { std::move(callback).Run(&socket1); })),
          Return(1)));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _, _))
      .WillOnce(DoAll(
          WithArgs<2>(Invoke(
              [&](const base::Callback<void(cast_channel::CastSocket * socket)>&
                      callback) { std::move(callback).Run(&socket2); })),
          Return(2)));

  // Invoke CastSocketService::OpenSocket on the IO thread.
  media_sink_service_impl_.OnDialSinkAdded(dial_sink1);
  base::RunLoop().RunUntilIdle();

  // Invoke CastSocketService::OpenSocket on the IO thread.
  media_sink_service_impl_.OnDialSinkAdded(dial_sink2);
  base::RunLoop().RunUntilIdle();
  // Verify sink content.
  EXPECT_EQ(2u, media_sink_service_impl_.current_sinks_map_.size());
}

TEST_F(CastMediaSinkServiceImplTest, TestOnFetchCompleted) {
  std::vector<MediaSinkInternal> sinks;
  EXPECT_CALL(mock_sink_discovered_cb_, Run(_)).WillOnce(SaveArg<0>(&sinks));

  auto cast_sink1 = CreateCastSink(1);
  auto cast_sink2 = CreateCastSink(2);
  auto cast_sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);

  // Find Cast sink 1, 2, 3
  media_sink_service_impl_.current_sinks_map_[ip_endpoint1.address()] =
      cast_sink1;
  media_sink_service_impl_.current_sinks_map_[ip_endpoint2.address()] =
      cast_sink2;
  media_sink_service_impl_.current_sinks_map_[ip_endpoint3.address()] =
      cast_sink3;

  // Callback returns Cast sink 1, 2, 3
  media_sink_service_impl_.OnFetchCompleted();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(base::ContainsValue(sinks, cast_sink1));
  EXPECT_TRUE(base::ContainsValue(sinks, cast_sink2));
  EXPECT_TRUE(base::ContainsValue(sinks, cast_sink3));
}

TEST_F(CastMediaSinkServiceImplTest, TestAttemptConnection) {
  auto cast_sink1 = CreateCastSink(1);
  auto cast_sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);

  // Find Cast sink 1
  media_sink_service_impl_.current_sinks_map_[ip_endpoint1.address()] =
      cast_sink1;

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _, _))
      .Times(0);
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _, _));

  // Attempt to connect to Cast sink 1, 2
  std::vector<MediaSinkInternal> sinks{cast_sink1, cast_sink2};
  media_sink_service_impl_.AttemptConnection(sinks);
}

TEST_F(CastMediaSinkServiceImplTest, CacheSinksForKnownNetwork) {
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllBlockingPoolTasksUntilIdle();

  MediaSinkInternal sink1 = CreateCastSink(1);
  MediaSinkInternal sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list1{sink1, sink2};

  // Resolution will succeed for both sinks.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocketInternal(&socket1);
  ExpectOpenSocketInternal(&socket2);
  media_sink_service_impl_.OpenChannels(sink_list1);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();

  fake_network_info_ = fake_wifi_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  content::RunAllBlockingPoolTasksUntilIdle();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint2);

  MediaSinkInternal sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  std::vector<MediaSinkInternal> sink_list2{sink3};

  cast_channel::MockCastSocket socket3;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  ExpectOpenSocketInternal(&socket3);
  media_sink_service_impl_.OpenChannels(sink_list2);

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _, _));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _, _));
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllBlockingPoolTasksUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, CacheContainsOnlyResolvedSinks) {
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllBlockingPoolTasksUntilIdle();

  MediaSinkInternal sink1 = CreateCastSink(1);
  MediaSinkInternal sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list1{sink1, sink2};

  // Resolution will fail for |sink2|.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  socket2.SetErrorState(cast_channel::ChannelError::CONNECT_ERROR);
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocketInternal(&socket1);
  ExpectOpenSocketInternal(&socket2);
  media_sink_service_impl_.OpenChannels(sink_list1);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();

  fake_network_info_ = fake_wifi_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  content::RunAllBlockingPoolTasksUntilIdle();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1);

  MediaSinkInternal sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  std::vector<MediaSinkInternal> sink_list2{sink3};

  cast_channel::MockCastSocket socket3;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  ExpectOpenSocketInternal(&socket3);
  media_sink_service_impl_.OpenChannels(sink_list2);

  // Reconnecting to the previous ethernet network should restore only |sink1|,
  // since |sink2| failed to resolve.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _, _));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _, _))
      .Times(0);
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllBlockingPoolTasksUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, CacheUpdatedOnChannelOpenFailed) {
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllBlockingPoolTasksUntilIdle();

  MediaSinkInternal sink1 = CreateCastSink(1);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  std::vector<MediaSinkInternal> sink_list1{sink1};

  // Resolve |sink1| but then raise a channel error.  This should remove it from
  // the cached sinks for the ethernet network.
  cast_channel::MockCastSocket socket1;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  ExpectOpenSocketInternal(&socket1);
  media_sink_service_impl_.OpenChannels(sink_list1);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();

  fake_network_info_ = fake_wifi_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  content::RunAllBlockingPoolTasksUntilIdle();

  MediaSinkInternal sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list2{sink2};

  cast_channel::MockCastSocket socket2;
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocketInternal(&socket2);
  media_sink_service_impl_.OpenChannels(sink_list2);

  // Reconnecting to the previous ethernet network should not restore any sinks
  // since the only sink to resolve successfully, |sink1|, later had a channel
  // error.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocketInternal(_, _, _, _))
      .Times(0);
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllBlockingPoolTasksUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, UnknownNetworkNoCache) {
  // Without any network notification here, network ID will remain __unknown__
  // and the cache shouldn't save any of these sinks.
  MediaSinkInternal sink1 = CreateCastSink(1);
  MediaSinkInternal sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list1{sink1, sink2};

  // Resolution will succeed for both sinks.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocketInternal(&socket1);
  ExpectOpenSocketInternal(&socket2);
  media_sink_service_impl_.OpenChannels(sink_list1);

  // Network is reported as disconnected but discover a new device.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint2);

  MediaSinkInternal sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  std::vector<MediaSinkInternal> sink_list2{sink3};

  cast_channel::MockCastSocket socket3;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  ExpectOpenSocketInternal(&socket3);
  media_sink_service_impl_.OpenChannels(sink_list2);

  // Connecting to a network whose ID resolves to __unknown__ shouldn't pull any
  // cache items from another unknown network.
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocketInternal(_, _, _, _))
      .Times(0);
  fake_network_info_ = fake_unknown_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  content::RunAllBlockingPoolTasksUntilIdle();

  // Similarly, disconnecting from the network shouldn't pull any cache items.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, CacheUpdatedForKnownNetwork) {
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllBlockingPoolTasksUntilIdle();

  MediaSinkInternal sink1 = CreateCastSink(1);
  MediaSinkInternal sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list1{sink1, sink2};

  // Resolution will succeed for both sinks.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocketInternal(&socket1);
  ExpectOpenSocketInternal(&socket2);
  media_sink_service_impl_.OpenChannels(sink_list1);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();

  fake_network_info_ = fake_wifi_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  content::RunAllBlockingPoolTasksUntilIdle();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint2);

  MediaSinkInternal sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  std::vector<MediaSinkInternal> sink_list2{sink3};

  cast_channel::MockCastSocket socket3;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  ExpectOpenSocketInternal(&socket3);
  media_sink_service_impl_.OpenChannels(sink_list2);

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them.  |sink3| is also lost.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();

  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint3);

  // Resolution will fail for cached sinks.
  socket1.SetErrorState(cast_channel::ChannelError::CONNECT_ERROR);
  socket2.SetErrorState(cast_channel::ChannelError::CONNECT_ERROR);
  ExpectOpenSocketInternal(&socket1);
  ExpectOpenSocketInternal(&socket2);
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllBlockingPoolTasksUntilIdle();

  // A new sink is found on the ethernet network.
  MediaSinkInternal sink4 = CreateCastSink(4);
  net::IPEndPoint ip_endpoint4 = CreateIPEndPoint(4);
  std::vector<MediaSinkInternal> sink_list3{sink4};

  cast_channel::MockCastSocket socket4;
  socket4.SetIPEndpoint(ip_endpoint4);
  socket4.set_id(4);
  ExpectOpenSocketInternal(&socket4);
  media_sink_service_impl_.OpenChannels(sink_list3);

  // Disconnect from the network and lose sinks.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint4);

  // Reconnect and expect only |sink4| to be cached.
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint4, _, _, _));
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllBlockingPoolTasksUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, CacheDialDiscoveredSinks) {
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllBlockingPoolTasksUntilIdle();

  MediaSinkInternal sink1_cast = CreateCastSink(1);
  MediaSinkInternal sink2_dial = CreateDialSink(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2(sink2_dial.dial_data().ip_address,
                               CastMediaSinkServiceImpl::kCastControlPort);
  std::vector<MediaSinkInternal> sink_list1{sink1_cast};

  // Resolution will succeed for both sinks.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocketInternal(&socket1);
  ExpectOpenSocketInternal(&socket2);
  media_sink_service_impl_.OpenChannels(sink_list1);
  media_sink_service_impl_.OnDialSinkAdded(sink2_dial);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();

  fake_network_info_ = fake_wifi_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  content::RunAllBlockingPoolTasksUntilIdle();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint2);

  MediaSinkInternal sink3_cast = CreateCastSink(3);
  MediaSinkInternal sink4_dial = CreateDialSink(4);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  net::IPEndPoint ip_endpoint4(sink4_dial.dial_data().ip_address,
                               CastMediaSinkServiceImpl::kCastControlPort);
  std::vector<MediaSinkInternal> sink_list2{sink3_cast};

  cast_channel::MockCastSocket socket3;
  cast_channel::MockCastSocket socket4;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  socket4.SetIPEndpoint(ip_endpoint4);
  socket4.set_id(4);
  ExpectOpenSocketInternal(&socket3);
  ExpectOpenSocketInternal(&socket4);
  media_sink_service_impl_.OpenChannels(sink_list2);
  media_sink_service_impl_.OnDialSinkAdded(sink4_dial);

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _, _));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _, _));
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllBlockingPoolTasksUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, DualDiscoveryDoesntDuplicateCacheItems) {
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllBlockingPoolTasksUntilIdle();

  // The same sink will be discovered via dial and mdns.
  MediaSinkInternal sink1_cast = CreateCastSink(1);
  MediaSinkInternal sink1_dial = CreateDialSink(1);
  net::IPEndPoint ip_endpoint1_cast = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint1_dial(sink1_dial.dial_data().ip_address,
                                    CastMediaSinkServiceImpl::kCastControlPort);
  std::vector<MediaSinkInternal> sink_list1{sink1_cast};

  // Dial discovery will succeed first.
  cast_channel::MockCastSocket socket1_dial;
  socket1_dial.SetIPEndpoint(ip_endpoint1_dial);
  socket1_dial.set_id(1);
  ExpectOpenSocketInternal(&socket1_dial);
  media_sink_service_impl_.OnDialSinkAdded(sink1_dial);

  // The same sink is then discovered via mdns.
  cast_channel::MockCastSocket socket1_cast;
  socket1_cast.SetIPEndpoint(ip_endpoint1_cast);
  socket1_cast.set_id(2);
  ExpectOpenSocketInternal(&socket1_cast);
  media_sink_service_impl_.OpenChannels(sink_list1);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();

  fake_network_info_ = fake_wifi_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  content::RunAllBlockingPoolTasksUntilIdle();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1_cast);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1_dial);

  MediaSinkInternal sink2_cast = CreateCastSink(2);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list2{sink2_cast};

  cast_channel::MockCastSocket socket2;
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(3);
  ExpectOpenSocketInternal(&socket2);
  media_sink_service_impl_.OpenChannels(sink_list2);

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllBlockingPoolTasksUntilIdle();

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1_cast, _, _, _));
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllBlockingPoolTasksUntilIdle();
}

}  // namespace media_router
