// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/media/router/test_helper.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithArgs;
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
  extra_data.ip_address = ip_endpoint.address();
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
        media_sink_service_impl_(mock_sink_discovered_cb_.Get(),
                                 mock_cast_socket_service_.get()) {}

  void SetUp() override {
    auto mock_timer = base::MakeUnique<base::MockTimer>(
        true /*retain_user_task*/, false /*is_repeating*/);
    mock_timer_ = mock_timer.get();
    media_sink_service_impl_.SetTimerForTest(std::move(mock_timer));
  }

 protected:
  const content::TestBrowserThreadBundle thread_bundle_;
  base::MockCallback<MediaSinkService::OnSinksDiscoveredCallback>
      mock_sink_discovered_cb_;
  std::unique_ptr<cast_channel::MockCastSocketService>
      mock_cast_socket_service_;
  CastMediaSinkServiceImpl media_sink_service_impl_;
  base::MockTimer* mock_timer_;

  DISALLOW_COPY_AND_ASSIGN(CastMediaSinkServiceImplTest);
};

TEST_F(CastMediaSinkServiceImplTest, TestOnChannelOpened) {
  auto cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);

  media_sink_service_impl_.current_service_ip_endpoints_.insert(ip_endpoint1);
  media_sink_service_impl_.OnChannelOpened(cast_sink, &socket);

  // Verify sink content
  EXPECT_CALL(mock_sink_discovered_cb_,
              Run(std::vector<MediaSinkInternal>({cast_sink})));
  media_sink_service_impl_.OnFetchCompleted();
}

TEST_F(CastMediaSinkServiceImplTest, TestMultipleOnChannelOpened) {
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
  media_sink_service_impl_.current_service_ip_endpoints_.insert(ip_endpoint1);
  media_sink_service_impl_.current_service_ip_endpoints_.insert(ip_endpoint2);
  // Fail to open channel 1.
  media_sink_service_impl_.OnChannelOpened(cast_sink2, &socket2);
  media_sink_service_impl_.OnChannelOpened(cast_sink3, &socket3);

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

  media_sink_service_impl_.current_service_ip_endpoints_.insert(ip_endpoint2);
  media_sink_service_impl_.OnChannelOpened(cast_sink2, &socket2);

  std::vector<MediaSinkInternal> sinks;
  EXPECT_CALL(mock_sink_discovered_cb_, Run(_)).WillOnce(SaveArg<0>(&sinks));

  // Fire timer.
  mock_timer_->Fire();
  EXPECT_EQ(sinks, std::vector<MediaSinkInternal>({cast_sink2}));

  EXPECT_FALSE(mock_timer_->IsRunning());
  // Channel 1 is opened and timer is restarted.
  cast_channel::MockCastSocket socket1;
  socket1.set_id(1);

  media_sink_service_impl_.current_service_ip_endpoints_.insert(ip_endpoint1);
  media_sink_service_impl_.OnChannelOpened(cast_sink1, &socket1);
  EXPECT_TRUE(mock_timer_->IsRunning());
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
  media_sink_service_impl_.OnChannelOpened(cast_sink2, &socket2);

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
  media_sink_service_impl_.OnChannelOpened(cast_sink1, &socket1);
  media_sink_service_impl_.OnChannelOpened(cast_sink3, &socket3);

  EXPECT_CALL(mock_sink_discovered_cb_,
              Run(std::vector<MediaSinkInternal>(
                  {cast_sink1, cast_sink2, cast_sink3})));
  media_sink_service_impl_.OnFetchCompleted();
}

TEST_F(CastMediaSinkServiceImplTest, TestOnChannelError) {
  auto cast_sink = CreateCastSink(1);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  cast_channel::MockCastSocket socket;
  socket.set_id(1);

  media_sink_service_impl_.current_service_ip_endpoints_.insert(ip_endpoint1);
  media_sink_service_impl_.OnChannelOpened(cast_sink, &socket);

  EXPECT_EQ(1u, media_sink_service_impl_.current_sinks_by_mdns_.size());

  socket.SetIPEndpoint(ip_endpoint1);
  media_sink_service_impl_.OnError(
      socket, cast_channel::ChannelError::CHANNEL_NOT_OPEN);
  EXPECT_TRUE(media_sink_service_impl_.current_sinks_by_mdns_.empty());
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
  EXPECT_EQ(2u, media_sink_service_impl_.current_sinks_by_dial_.size());
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

  // Cast sink 1, 2 from mDNS discovery
  media_sink_service_impl_.current_sinks_by_mdns_[ip_endpoint1] = cast_sink1;
  media_sink_service_impl_.current_sinks_by_mdns_[ip_endpoint2] = cast_sink2;
  // Cast sink 2, 3 from dial discovery
  media_sink_service_impl_.current_sinks_by_dial_[ip_endpoint2] = cast_sink2;
  media_sink_service_impl_.current_sinks_by_dial_[ip_endpoint3] = cast_sink3;

  // Callback returns Cast sink 1, 2, 3
  media_sink_service_impl_.OnFetchCompleted();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(base::ContainsValue(sinks, cast_sink1));
  EXPECT_TRUE(base::ContainsValue(sinks, cast_sink2));
  EXPECT_TRUE(base::ContainsValue(sinks, cast_sink3));
}

}  // namespace media_router
