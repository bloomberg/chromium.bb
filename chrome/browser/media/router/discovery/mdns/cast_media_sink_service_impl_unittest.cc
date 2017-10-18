// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/media/router/media_router_feature.h"
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
using base::Bucket;
using cast_channel::ChannelError;
using testing::ElementsAre;

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

MATCHER_P(RetryParamEq, expected, "") {
  return expected.initial_delay_in_milliseconds ==
             arg.initial_delay_in_milliseconds &&
         expected.max_retry_attempts == arg.max_retry_attempts &&
         expected.multiply_factor == arg.multiply_factor;
}

MATCHER_P(OpenParamEq, expected, "") {
  return expected.connect_timeout_in_seconds ==
             arg.connect_timeout_in_seconds &&
         expected.dynamic_timeout_delta_in_seconds ==
             arg.dynamic_timeout_delta_in_seconds &&
         expected.liveness_timeout_in_seconds ==
             arg.liveness_timeout_in_seconds &&
         expected.ping_interval_in_seconds == arg.ping_interval_in_seconds;
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
                                 nullptr /* url_request_context_getter */,
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
                OpenSocketInternal(socket->ip_endpoint(), _, _))
        .WillOnce(Invoke(
            [socket](const auto& ip_endpoint, auto* net_log, auto open_cb) {
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

  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink, &socket, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Verify sink content
  EXPECT_CALL(mock_sink_discovered_cb_,
              Run(std::vector<MediaSinkInternal>({cast_sink})));
  media_sink_service_impl_.OnFetchCompleted();
}

TEST_F(CastMediaSinkServiceImplTest, TestMultipleOnChannelOpenSucceeded) {
  MediaSinkInternal cast_sink1 = CreateCastSink(1);
  MediaSinkInternal cast_sink2 = CreateCastSink(2);
  MediaSinkInternal cast_sink3 = CreateCastSink(3);

  CastSinkExtraData extra_data = cast_sink3.cast_data();
  extra_data.discovered_by_dial = true;
  cast_sink3.set_cast_data(extra_data);

  cast_channel::MockCastSocket socket2;
  socket2.set_id(2);
  cast_channel::MockCastSocket socket3;
  socket3.set_id(3);

  // Current round of Dns discovery finds service1 and service 2.
  // Fail to open channel 1.
  base::HistogramTester tester;
  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink2, &socket2, CastMediaSinkServiceImpl::SinkSource::kMdns);
  EXPECT_THAT(
      tester.GetAllSamples(
          CastDeviceCountMetrics::kHistogramCastDiscoverySinkSource),
      ElementsAre(Bucket(
          static_cast<int>(CastMediaSinkServiceImpl::SinkSource::kMdns), 1)));

  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink3, &socket3, CastMediaSinkServiceImpl::SinkSource::kDial);
  EXPECT_THAT(
      tester.GetAllSamples(
          CastDeviceCountMetrics::kHistogramCastDiscoverySinkSource),
      ElementsAre(
          Bucket(static_cast<int>(CastMediaSinkServiceImpl::SinkSource::kMdns),
                 1),
          Bucket(static_cast<int>(CastMediaSinkServiceImpl::SinkSource::kDial),
                 1)));

  extra_data.discovered_by_dial = false;
  cast_sink3.set_cast_data(extra_data);
  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink3, &socket3, CastMediaSinkServiceImpl::SinkSource::kMdns);
  EXPECT_THAT(
      tester.GetAllSamples(
          CastDeviceCountMetrics::kHistogramCastDiscoverySinkSource),
      ElementsAre(
          Bucket(static_cast<int>(CastMediaSinkServiceImpl::SinkSource::kMdns),
                 1),
          Bucket(static_cast<int>(CastMediaSinkServiceImpl::SinkSource::kDial),
                 1),
          Bucket(
              static_cast<int>(CastMediaSinkServiceImpl::SinkSource::kDialMdns),
              1)));

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

  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink2, &socket2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  std::vector<MediaSinkInternal> sinks;
  EXPECT_CALL(mock_sink_discovered_cb_, Run(_)).WillOnce(SaveArg<0>(&sinks));

  // Fire timer.
  mock_timer_->Fire();
  EXPECT_EQ(sinks, std::vector<MediaSinkInternal>({cast_sink2}));

  EXPECT_FALSE(mock_timer_->IsRunning());
  // Channel 1 is opened and timer is restarted.
  cast_channel::MockCastSocket socket1;
  socket1.set_id(1);

  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink1, &socket1, CastMediaSinkServiceImpl::SinkSource::kMdns);
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
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocketInternal(ip_endpoint, _, _))
      .Times(1);
  media_sink_service_impl_.OpenChannel(
      ip_endpoint, cast_sink, nullptr,
      CastMediaSinkServiceImpl::SinkSource::kMdns);

  // One pending sink, the same as |cast_sink|
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocketInternal(ip_endpoint, _, _))
      .Times(0);
  media_sink_service_impl_.OpenChannel(
      ip_endpoint, cast_sink, nullptr,
      CastMediaSinkServiceImpl::SinkSource::kMdns);
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
      new net::BackoffEntry(&media_sink_service_impl_.backoff_policy_));
  media_sink_service_impl_.retry_params_.max_retry_attempts = 3;
  ExpectOpenSocketInternal(&socket);
  media_sink_service_impl_.OpenChannel(
      ip_endpoint, cast_sink, std::move(backoff_entry),
      CastMediaSinkServiceImpl::SinkSource::kMdns);

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
  std::unique_ptr<net::BackoffEntry> backoff_entry(
      new net::BackoffEntry(&media_sink_service_impl_.backoff_policy_));
  media_sink_service_impl_.retry_params_.max_retry_attempts = 3;
  auto* backoff_entry_ptr = backoff_entry.get();
  media_sink_service_impl_.OpenChannel(
      ip_endpoint, cast_sink, std::move(backoff_entry),
      CastMediaSinkServiceImpl::SinkSource::kMdns);

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
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocketInternal(ip_endpoint, _, _))
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

  base::SimpleTestClock* clock = new base::SimpleTestClock();
  base::Time start_time = base::Time::Now();
  clock->SetNow(start_time);
  media_sink_service_impl_.SetClockForTest(base::WrapUnique(clock));

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _));

  // 1st round finds service 1 & 2.
  std::vector<MediaSinkInternal> sinks1{cast_sink1, cast_sink2};
  media_sink_service_impl_.OpenChannels(
      sinks1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Channel 2 opened.
  cast_channel::MockCastSocket socket2;
  socket2.set_id(2);
  socket2.SetErrorState(cast_channel::ChannelError::NONE);

  base::TimeDelta delta = base::TimeDelta::FromSeconds(2);
  clock->Advance(delta);
  base::HistogramTester tester;

  media_sink_service_impl_.OnChannelOpened(
      cast_sink2, nullptr, CastMediaSinkServiceImpl::SinkSource::kMdns,
      start_time, &socket2);
  tester.ExpectUniqueSample(CastAnalytics::kHistogramCastMdnsChannelOpenSuccess,
                            delta.InMilliseconds(), 1);

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint3, _, _));

  // 2nd round finds service 2 & 3.
  std::vector<MediaSinkInternal> sinks2{cast_sink2, cast_sink3};
  media_sink_service_impl_.OpenChannels(
      sinks2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Channel 1 and 3 opened.
  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket3;
  socket1.set_id(1);
  socket3.set_id(3);
  socket1.SetErrorState(cast_channel::ChannelError::NONE);
  socket3.SetErrorState(cast_channel::ChannelError::NONE);
  media_sink_service_impl_.OnChannelOpened(
      cast_sink1, nullptr, CastMediaSinkServiceImpl::SinkSource::kMdns,
      start_time, &socket1);
  media_sink_service_impl_.OnChannelOpened(
      cast_sink3, nullptr, CastMediaSinkServiceImpl::SinkSource::kMdns,
      start_time, &socket3);

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

  media_sink_service_impl_.OnChannelOpenSucceeded(
      cast_sink, &socket, CastMediaSinkServiceImpl::SinkSource::kMdns);

  EXPECT_EQ(1u, media_sink_service_impl_.current_sinks_map_.size());
  EXPECT_TRUE(media_sink_service_impl_.failure_count_map_.empty());

  socket.SetIPEndpoint(ip_endpoint1);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1);
  EXPECT_TRUE(media_sink_service_impl_.current_sinks_map_.empty());
  EXPECT_EQ(1, media_sink_service_impl_.failure_count_map_[ip_endpoint1]);

  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1);
  EXPECT_EQ(2, media_sink_service_impl_.failure_count_map_[ip_endpoint1]);
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
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocketInternal(_, _, _)).Times(0);

  base::HistogramTester tester;
  media_sink_service_impl_.OnError(
      socket, cast_channel::ChannelError::CHANNEL_NOT_OPEN);

  tester.ExpectTotalCount(CastAnalytics::kHistogramCastChannelError, 1);
  EXPECT_THAT(tester.GetAllSamples(CastAnalytics::kHistogramCastChannelError),
              ElementsAre(Bucket(
                  static_cast<int>(MediaRouterChannelError::UNKNOWN), 1)));
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
  media_sink_service_impl_.current_sinks_map_[ip_endpoint1] = cast_sink;
  EXPECT_CALL(socket, ready_state())
      .WillRepeatedly(Return(cast_channel::ReadyState::CLOSED));
  media_sink_service_impl_.OnError(
      socket, cast_channel::ChannelError::CHANNEL_NOT_OPEN);

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _));
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
              OpenSocketInternal(ip_endpoint1, _, _))
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
              OpenSocketInternal(ip_endpoint1, _, _))
      .WillOnce(DoAll(
          WithArgs<2>(Invoke(
              [&](const base::Callback<void(cast_channel::CastSocket * socket)>&
                      callback) { std::move(callback).Run(&socket1); })),
          Return(1)));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _))
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

TEST_F(CastMediaSinkServiceImplTest, TestOnDialSinkAddedSkipsIfNonCastDevice) {
  MediaSinkInternal dial_sink1 = CreateDialSink(1);
  net::IPEndPoint ip_endpoint1(dial_sink1.dial_data().ip_address,
                               CastMediaSinkServiceImpl::kCastControlPort);

  cast_channel::MockCastSocket socket1;
  socket1.set_id(1);
  socket1.SetErrorState(cast_channel::ChannelError::CONNECT_ERROR);

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _))
      .Times(1)
      .WillOnce(DoAll(
          WithArgs<2>(Invoke(
              [&socket1](
                  const base::Callback<void(cast_channel::CastSocket * socket)>&
                      callback) { std::move(callback).Run(&socket1); })),
          Return(1)));
  media_sink_service_impl_.OnDialSinkAdded(dial_sink1);

  // We don't trigger retries, thus each iteration will only increment the
  // failure count once.
  for (int i = 0; i < CastMediaSinkServiceImpl::kMaxDialSinkFailureCount - 1;
       ++i) {
    EXPECT_CALL(*mock_cast_socket_service_,
                OpenSocketInternal(ip_endpoint1, _, _))
        .Times(1)
        .WillOnce(DoAll(
            WithArgs<2>(Invoke(
                [&socket1](const base::Callback<void(cast_channel::CastSocket *
                                                     socket)>& callback) {
                  std::move(callback).Run(&socket1);
                })),
            Return(1)));
    media_sink_service_impl_.OnDialSinkAdded(dial_sink1);
  }

  auto& dial_sink_failure_count =
      media_sink_service_impl_.dial_sink_failure_count_;
  auto failure_count_it = dial_sink_failure_count.find(ip_endpoint1.address());
  ASSERT_TRUE(failure_count_it != dial_sink_failure_count.end());
  int failure_count = failure_count_it->second;
  EXPECT_GE(failure_count, CastMediaSinkServiceImpl::kMaxDialSinkFailureCount);

  // OnChannelOpenFailed too many times; next time OnDialSinkAdded is called,
  // we won't attempt to open channel.
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _))
      .Times(0);

  media_sink_service_impl_.OnDialSinkAdded(dial_sink1);

  EXPECT_EQ(0u, media_sink_service_impl_.current_sinks_map_.size());

  // Same IP address as dial_sink1; thus it is considered to be the same device.
  // The outcome of the channel does not matter here; the sink is considered a
  // Cast device since it has been discovered via mDNS.
  MediaSinkInternal cast_sink = CreateCastSink(1);
  std::vector<MediaSinkInternal> cast_sinks = {cast_sink};
  ASSERT_EQ(ip_endpoint1.address(),
            cast_sink.cast_data().ip_endpoint.address());
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(cast_sink.cast_data().ip_endpoint, _, _))
      .Times(1);
  media_sink_service_impl_.OpenChannels(
      cast_sinks, CastMediaSinkServiceImpl::SinkSource::kMdns);

  failure_count_it = dial_sink_failure_count.find(ip_endpoint1.address());
  ASSERT_TRUE(failure_count_it == dial_sink_failure_count.end());

  // |dial_sink_failure_count| gets cleared on network change.
  dial_sink_failure_count[ip_endpoint1.address()] = 6;

  media_sink_service_impl_.OnNetworksChanged("anotherNetworkId");
  EXPECT_TRUE(dial_sink_failure_count.empty());
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
  media_sink_service_impl_.current_sinks_map_[ip_endpoint1] = cast_sink1;
  media_sink_service_impl_.current_sinks_map_[ip_endpoint2] = cast_sink2;
  media_sink_service_impl_.current_sinks_map_[ip_endpoint3] = cast_sink3;

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
  media_sink_service_impl_.current_sinks_map_[ip_endpoint1] = cast_sink1;

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _))
      .Times(0);
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _));

  // Attempt to connect to Cast sink 1, 2
  std::vector<MediaSinkInternal> sinks{cast_sink1, cast_sink2};
  media_sink_service_impl_.AttemptConnection(sinks);
}

TEST_F(CastMediaSinkServiceImplTest, CacheSinksForKnownNetwork) {
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();

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
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();

  fake_network_info_ = fake_wifi_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint2);

  MediaSinkInternal sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  std::vector<MediaSinkInternal> sink_list2{sink3};

  cast_channel::MockCastSocket socket3;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  ExpectOpenSocketInternal(&socket3);
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _));
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, CacheContainsOnlyResolvedSinks) {
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();

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
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();

  fake_network_info_ = fake_wifi_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1);

  MediaSinkInternal sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  std::vector<MediaSinkInternal> sink_list2{sink3};

  cast_channel::MockCastSocket socket3;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  ExpectOpenSocketInternal(&socket3);
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Reconnecting to the previous ethernet network should restore only |sink1|,
  // since |sink2| failed to resolve.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _))
      .Times(0);
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, CacheUpdatedOnChannelOpenFailed) {
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();

  MediaSinkInternal sink1 = CreateCastSink(1);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  std::vector<MediaSinkInternal> sink_list1{sink1};

  // Resolve |sink1| but then raise a channel error.  This should remove it from
  // the cached sinks for the ethernet network.
  cast_channel::MockCastSocket socket1;
  socket1.SetIPEndpoint(ip_endpoint1);
  socket1.set_id(1);
  ExpectOpenSocketInternal(&socket1);
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();

  fake_network_info_ = fake_wifi_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();

  MediaSinkInternal sink2 = CreateCastSink(2);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list2{sink2};

  cast_channel::MockCastSocket socket2;
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(2);
  ExpectOpenSocketInternal(&socket2);
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Reconnecting to the previous ethernet network should not restore any sinks
  // since the only sink to resolve successfully, |sink1|, later had a channel
  // error.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();

  EXPECT_CALL(*mock_cast_socket_service_, OpenSocketInternal(_, _, _)).Times(0);
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
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
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Network is reported as disconnected but discover a new device.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint2);

  MediaSinkInternal sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  std::vector<MediaSinkInternal> sink_list2{sink3};

  cast_channel::MockCastSocket socket3;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  ExpectOpenSocketInternal(&socket3);
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Connecting to a network whose ID resolves to __unknown__ shouldn't pull any
  // cache items from another unknown network.
  EXPECT_CALL(*mock_cast_socket_service_, OpenSocketInternal(_, _, _)).Times(0);
  fake_network_info_ = fake_unknown_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();

  // Similarly, disconnecting from the network shouldn't pull any cache items.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, CacheUpdatedForKnownNetwork) {
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();

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
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();

  fake_network_info_ = fake_wifi_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint2);

  MediaSinkInternal sink3 = CreateCastSink(3);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);
  std::vector<MediaSinkInternal> sink_list2{sink3};

  cast_channel::MockCastSocket socket3;
  socket3.SetIPEndpoint(ip_endpoint3);
  socket3.set_id(3);
  ExpectOpenSocketInternal(&socket3);
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them.  |sink3| is also lost.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();

  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint3);

  // Resolution will fail for cached sinks.
  socket1.SetErrorState(cast_channel::ChannelError::CONNECT_ERROR);
  socket2.SetErrorState(cast_channel::ChannelError::CONNECT_ERROR);
  ExpectOpenSocketInternal(&socket1);
  ExpectOpenSocketInternal(&socket2);
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();

  // A new sink is found on the ethernet network.
  MediaSinkInternal sink4 = CreateCastSink(4);
  net::IPEndPoint ip_endpoint4 = CreateIPEndPoint(4);
  std::vector<MediaSinkInternal> sink_list3{sink4};

  cast_channel::MockCastSocket socket4;
  socket4.SetIPEndpoint(ip_endpoint4);
  socket4.set_id(4);
  ExpectOpenSocketInternal(&socket4);
  media_sink_service_impl_.OpenChannels(
      sink_list3, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Disconnect from the network and lose sinks.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint4);

  // Reconnect and expect only |sink4| to be cached.
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint4, _, _));
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, CacheDialDiscoveredSinks) {
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();

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
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);
  media_sink_service_impl_.OnDialSinkAdded(sink2_dial);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();

  fake_network_info_ = fake_wifi_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
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
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);
  media_sink_service_impl_.OnDialSinkAdded(sink4_dial);

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _));
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, DualDiscoveryDoesntDuplicateCacheItems) {
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();

  // The same sink will be discovered via dial and mdns.
  MediaSinkInternal sink1_cast = CreateCastSink(0);
  MediaSinkInternal sink1_dial = CreateDialSink(0);
  net::IPEndPoint ip_endpoint1_cast = CreateIPEndPoint(0);
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
  media_sink_service_impl_.OpenChannels(
      sink_list1, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Connect to a new network with different sinks.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();

  fake_network_info_ = fake_wifi_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  content::RunAllTasksUntilIdle();
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1_cast);
  media_sink_service_impl_.OnChannelOpenFailed(ip_endpoint1_dial);

  MediaSinkInternal sink2_cast = CreateCastSink(2);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  std::vector<MediaSinkInternal> sink_list2{sink2_cast};

  cast_channel::MockCastSocket socket2;
  socket2.SetIPEndpoint(ip_endpoint2);
  socket2.set_id(3);
  ExpectOpenSocketInternal(&socket2);
  media_sink_service_impl_.OpenChannels(
      sink_list2, CastMediaSinkServiceImpl::SinkSource::kMdns);

  // Reconnecting to the previous ethernet network should restore the same sinks
  // from the cache and attempt to resolve them.
  fake_network_info_.clear();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  content::RunAllTasksUntilIdle();

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1_cast, _, _));
  fake_network_info_ = fake_ethernet_info_;
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  content::RunAllTasksUntilIdle();
}

TEST_F(CastMediaSinkServiceImplTest, TestCreateCastSocketOpenParams) {
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(1);
  int connect_timeout_in_seconds =
      media_sink_service_impl_.open_params_.connect_timeout_in_seconds;
  int liveness_timeout_in_seconds =
      media_sink_service_impl_.open_params_.liveness_timeout_in_seconds;
  int delta_in_seconds = 5;
  media_sink_service_impl_.open_params_.dynamic_timeout_delta_in_seconds =
      delta_in_seconds;

  // No error
  auto open_params =
      media_sink_service_impl_.CreateCastSocketOpenParams(ip_endpoint);
  EXPECT_EQ(connect_timeout_in_seconds,
            open_params.connect_timeout.InSeconds());
  EXPECT_EQ(liveness_timeout_in_seconds,
            open_params.liveness_timeout.InSeconds());

  // One error
  connect_timeout_in_seconds += delta_in_seconds;
  liveness_timeout_in_seconds += delta_in_seconds;
  media_sink_service_impl_.failure_count_map_[ip_endpoint] = 1;
  open_params =
      media_sink_service_impl_.CreateCastSocketOpenParams(ip_endpoint);
  EXPECT_EQ(connect_timeout_in_seconds,
            open_params.connect_timeout.InSeconds());
  EXPECT_EQ(liveness_timeout_in_seconds,
            open_params.liveness_timeout.InSeconds());

  // Two errors
  connect_timeout_in_seconds += delta_in_seconds;
  liveness_timeout_in_seconds += delta_in_seconds;
  media_sink_service_impl_.failure_count_map_[ip_endpoint] = 2;
  open_params =
      media_sink_service_impl_.CreateCastSocketOpenParams(ip_endpoint);
  EXPECT_EQ(connect_timeout_in_seconds,
            open_params.connect_timeout.InSeconds());
  EXPECT_EQ(liveness_timeout_in_seconds,
            open_params.liveness_timeout.InSeconds());

  // Ten errors
  connect_timeout_in_seconds = 30;
  liveness_timeout_in_seconds = 60;
  media_sink_service_impl_.failure_count_map_[ip_endpoint] = 10;
  open_params =
      media_sink_service_impl_.CreateCastSocketOpenParams(ip_endpoint);
  EXPECT_EQ(connect_timeout_in_seconds,
            open_params.connect_timeout.InSeconds());
  EXPECT_EQ(liveness_timeout_in_seconds,
            open_params.liveness_timeout.InSeconds());
}

TEST_F(CastMediaSinkServiceImplTest,
       TestInitRetryParametersWithFeatureDisabled) {
  // Feature not enabled.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kEnableCastDiscovery);

  EXPECT_THAT(CastMediaSinkServiceImpl::RetryParams::GetFromFieldTrialParam(),
              RetryParamEq(CastMediaSinkServiceImpl::RetryParams()));
  EXPECT_THAT(CastMediaSinkServiceImpl::OpenParams::GetFromFieldTrialParam(),
              OpenParamEq(CastMediaSinkServiceImpl::OpenParams()));
}

TEST_F(CastMediaSinkServiceImplTest, TestInitParameters) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::map<std::string, std::string> params;
  params["initial_delay_in_ms"] = "2000";
  params["max_retry_attempts"] = "20";
  params["exponential"] = "2.0";

  params["connect_timeout_in_seconds"] = "20";
  params["ping_interval_in_seconds"] = "15";
  params["liveness_timeout_in_seconds"] = "30";
  params["dynamic_timeout_delta_in_seconds"] = "7";
  scoped_feature_list.InitAndEnableFeatureWithParameters(kEnableCastDiscovery,
                                                         params);

  CastMediaSinkServiceImpl::RetryParams expected_retry_params;
  expected_retry_params.initial_delay_in_milliseconds = 2000;
  expected_retry_params.max_retry_attempts = 20;
  expected_retry_params.multiply_factor = 2.0;
  EXPECT_THAT(CastMediaSinkServiceImpl::RetryParams::GetFromFieldTrialParam(),
              RetryParamEq(expected_retry_params));

  CastMediaSinkServiceImpl::OpenParams expected_open_params;
  expected_open_params.connect_timeout_in_seconds = 20;
  expected_open_params.ping_interval_in_seconds = 15;
  expected_open_params.liveness_timeout_in_seconds = 30;
  expected_open_params.dynamic_timeout_delta_in_seconds = 7;
  EXPECT_THAT(CastMediaSinkServiceImpl::OpenParams::GetFromFieldTrialParam(),
              OpenParamEq(expected_open_params));
}

TEST_F(CastMediaSinkServiceImplTest, TestInitRetryParametersWithDefaultValue) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableCastDiscovery);

  EXPECT_THAT(CastMediaSinkServiceImpl::RetryParams::GetFromFieldTrialParam(),
              RetryParamEq(CastMediaSinkServiceImpl::RetryParams()));
  EXPECT_THAT(CastMediaSinkServiceImpl::OpenParams::GetFromFieldTrialParam(),
              OpenParamEq(CastMediaSinkServiceImpl::OpenParams()));
}

}  // namespace media_router
