// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/media/router/discovery/mdns/mock_dns_sd_registry.h"
#include "chrome/browser/media/router/test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

namespace {

net::IPEndPoint CreateIPEndPoint(int num) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(
      base::StringPrintf("192.168.0.10%d", num)));
  return net::IPEndPoint(ip_address, 8009 + num);
}

media_router::DnsSdService CreateDnsService(int num) {
  net::IPEndPoint ip_endpoint = CreateIPEndPoint(num);
  media_router::DnsSdService service;
  service.service_name =
      "_myDevice." +
      std::string(media_router::CastMediaSinkService::kCastServiceType);
  service.ip_address = ip_endpoint.address().ToString();
  service.service_host_port = ip_endpoint.ToString();
  service.service_data.push_back(base::StringPrintf("id=service %d", num));
  service.service_data.push_back(
      base::StringPrintf("fn=friendly name %d", num));
  service.service_data.push_back(base::StringPrintf("md=model name %d", num));

  return service;
}

void VerifyMediaSinkInternal(const media_router::MediaSinkInternal& cast_sink,
                             const media_router::DnsSdService& service,
                             int channel_id,
                             bool audio_only) {
  std::string id = base::StringPrintf("service %d", channel_id);
  std::string name = base::StringPrintf("friendly name %d", channel_id);
  std::string model_name = base::StringPrintf("model name %d", channel_id);
  EXPECT_EQ(id, cast_sink.sink().id());
  EXPECT_EQ(name, cast_sink.sink().name());
  EXPECT_EQ(model_name, cast_sink.cast_data().model_name);
  EXPECT_EQ(service.ip_address, cast_sink.cast_data().ip_address.ToString());

  int capabilities = cast_channel::CastDeviceCapability::AUDIO_OUT;
  if (!audio_only)
    capabilities |= cast_channel::CastDeviceCapability::VIDEO_OUT;
  EXPECT_EQ(capabilities, cast_sink.cast_data().capabilities);
  EXPECT_EQ(channel_id, cast_sink.cast_data().cast_channel_id);
}

}  // namespace

namespace media_router {

class CastMediaSinkServiceTest : public ::testing::Test {
 public:
  CastMediaSinkServiceTest()
      : mock_cast_socket_service_(new cast_channel::MockCastSocketService()),
        media_sink_service_(
            new CastMediaSinkService(mock_sink_discovered_cb_.Get(),
                                     mock_cast_socket_service_.get())),
        test_dns_sd_registry_(media_sink_service_.get()) {}

  void SetUp() override {
    auto mock_timer = base::MakeUnique<base::MockTimer>(
        true /*retain_user_task*/, false /*is_repeating*/);
    mock_timer_ = mock_timer.get();
    media_sink_service_->SetTimerForTest(std::move(mock_timer));
  }

 protected:
  const content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;

  base::MockCallback<MediaSinkService::OnSinksDiscoveredCallback>
      mock_sink_discovered_cb_;
  std::unique_ptr<cast_channel::MockCastSocketService>
      mock_cast_socket_service_;
  scoped_refptr<CastMediaSinkService> media_sink_service_;
  MockDnsSdRegistry test_dns_sd_registry_;
  base::MockTimer* mock_timer_;

  DISALLOW_COPY_AND_ASSIGN(CastMediaSinkServiceTest);
};

TEST_F(CastMediaSinkServiceTest, TestReStartAfterStop) {
  EXPECT_CALL(test_dns_sd_registry_, AddObserver(media_sink_service_.get()))
      .Times(2);
  EXPECT_CALL(test_dns_sd_registry_, RegisterDnsSdListener(_)).Times(2);
  EXPECT_FALSE(mock_timer_->IsRunning());
  media_sink_service_->SetDnsSdRegistryForTest(&test_dns_sd_registry_);
  media_sink_service_->Start();
  EXPECT_TRUE(mock_timer_->IsRunning());

  EXPECT_CALL(test_dns_sd_registry_, RemoveObserver(media_sink_service_.get()));
  EXPECT_CALL(test_dns_sd_registry_, UnregisterDnsSdListener(_));
  media_sink_service_->Stop();

  mock_timer_ =
      new base::MockTimer(true /*retain_user_task*/, false /*is_repeating*/);
  media_sink_service_->SetTimerForTest(base::WrapUnique(mock_timer_));
  media_sink_service_->SetDnsSdRegistryForTest(&test_dns_sd_registry_);
  media_sink_service_->Start();
  EXPECT_TRUE(mock_timer_->IsRunning());
}

TEST_F(CastMediaSinkServiceTest, TestMultipleStartAndStop) {
  EXPECT_CALL(test_dns_sd_registry_, AddObserver(media_sink_service_.get()));
  EXPECT_CALL(test_dns_sd_registry_, RegisterDnsSdListener(_));
  media_sink_service_->SetDnsSdRegistryForTest(&test_dns_sd_registry_);
  media_sink_service_->Start();
  media_sink_service_->Start();
  EXPECT_TRUE(mock_timer_->IsRunning());

  EXPECT_CALL(test_dns_sd_registry_, RemoveObserver(media_sink_service_.get()));
  EXPECT_CALL(test_dns_sd_registry_, UnregisterDnsSdListener(_));
  media_sink_service_->Stop();
  media_sink_service_->Stop();
}

TEST_F(CastMediaSinkServiceTest, TestOnChannelOpenedOnIOThread) {
  DnsSdService service = CreateDnsService(1);
  cast_channel::MockCastSocket socket;
  EXPECT_CALL(*mock_cast_socket_service_, GetSocket(1))
      .WillOnce(Return(&socket));

  media_sink_service_->current_services_.push_back(service);
  media_sink_service_->OnChannelOpenedOnIOThread(
      service, 1, cast_channel::ChannelError::NONE);
  // Invoke CastMediaSinkService::OnChannelOpenedOnUIThread on the UI thread.
  base::RunLoop().RunUntilIdle();

  // Verify sink content
  EXPECT_EQ(size_t(1), media_sink_service_->current_sinks_.size());
  for (const auto& sink_it : media_sink_service_->current_sinks_)
    VerifyMediaSinkInternal(sink_it, service, 1, false);
}

TEST_F(CastMediaSinkServiceTest, TestMultipleOnChannelOpenedOnIOThread) {
  DnsSdService service1 = CreateDnsService(1);
  DnsSdService service2 = CreateDnsService(2);
  DnsSdService service3 = CreateDnsService(3);

  cast_channel::MockCastSocket socket2;
  cast_channel::MockCastSocket socket3;
  // Fail to open channel 1.
  EXPECT_CALL(*mock_cast_socket_service_, GetSocket(1))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*mock_cast_socket_service_, GetSocket(2))
      .WillOnce(Return(&socket2));
  EXPECT_CALL(*mock_cast_socket_service_, GetSocket(3))
      .WillOnce(Return(&socket2));

  // Current round of Dns discovery finds service1 and service 2.
  media_sink_service_->current_services_.push_back(service1);
  media_sink_service_->current_services_.push_back(service2);
  media_sink_service_->OnChannelOpenedOnIOThread(
      service1, 1, cast_channel::ChannelError::NONE);
  media_sink_service_->OnChannelOpenedOnIOThread(
      service2, 2, cast_channel::ChannelError::NONE);
  media_sink_service_->OnChannelOpenedOnIOThread(
      service3, 3, cast_channel::ChannelError::NONE);
  // Invoke CastMediaSinkService::OnChannelOpenedOnUIThread on the UI thread.
  base::RunLoop().RunUntilIdle();

  // Verify sink content
  EXPECT_EQ(size_t(1), media_sink_service_->current_sinks_.size());
  for (const auto& sink_it : media_sink_service_->current_sinks_)
    VerifyMediaSinkInternal(sink_it, service2, 2, false);
}

TEST_F(CastMediaSinkServiceTest, TestOnDnsSdEvent) {
  DnsSdService service1 = CreateDnsService(1);
  DnsSdService service2 = CreateDnsService(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);

  // Add dns services.
  DnsSdRegistry::DnsSdServiceList service_list{service1, service2};

  cast_channel::MockCastSocket::MockOnOpenCallback callback1;
  cast_channel::MockCastSocket::MockOnOpenCallback callback2;
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&callback1), Return(1)));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&callback2), Return(2)));

  // Invoke CastSocketService::OpenSocket on the IO thread.
  media_sink_service_->OnDnsSdEvent(CastMediaSinkService::kCastServiceType,
                                    service_list);
  base::RunLoop().RunUntilIdle();

  cast_channel::MockCastSocket socket1;
  cast_channel::MockCastSocket socket2;

  EXPECT_CALL(*mock_cast_socket_service_, GetSocket(1))
      .WillOnce(Return(&socket1));
  EXPECT_CALL(*mock_cast_socket_service_, GetSocket(2))
      .WillOnce(Return(&socket2));

  callback1.Run(1, cast_channel::ChannelError::NONE);
  callback2.Run(2, cast_channel::ChannelError::NONE);

  // Invoke CastMediaSinkService::OnChannelOpenedOnUIThread on the UI thread.
  base::RunLoop().RunUntilIdle();
  // Verify sink content
  EXPECT_EQ(size_t(2), media_sink_service_->current_sinks_.size());
}

TEST_F(CastMediaSinkServiceTest, TestMultipleOnDnsSdEvent) {
  DnsSdService service1 = CreateDnsService(1);
  DnsSdService service2 = CreateDnsService(2);
  DnsSdService service3 = CreateDnsService(3);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);
  net::IPEndPoint ip_endpoint3 = CreateIPEndPoint(3);

  // 1st round finds service 1 & 2.
  DnsSdRegistry::DnsSdServiceList service_list1{service1, service2};
  media_sink_service_->OnDnsSdEvent(CastMediaSinkService::kCastServiceType,
                                    service_list1);

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _, _));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _, _));
  base::RunLoop().RunUntilIdle();

  // Channel 2 opened.
  media_sink_service_->OnChannelOpenedOnUIThread(service2, 2, false);

  // 2nd round finds service 2 & 3.
  DnsSdRegistry::DnsSdServiceList service_list2{service2, service3};
  media_sink_service_->OnDnsSdEvent(CastMediaSinkService::kCastServiceType,
                                    service_list2);

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _, _));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint3, _, _, _));
  base::RunLoop().RunUntilIdle();

  // Channel 1 and 3 opened.
  media_sink_service_->OnChannelOpenedOnUIThread(service1, 1, false);
  media_sink_service_->OnChannelOpenedOnUIThread(service3, 3, false);

  EXPECT_EQ(size_t(1), media_sink_service_->current_sinks_.size());
  for (const auto& sink_it : media_sink_service_->current_sinks_)
    VerifyMediaSinkInternal(sink_it, service3, 3, false);
}

TEST_F(CastMediaSinkServiceTest, TestTimer) {
  DnsSdService service1 = CreateDnsService(1);
  DnsSdService service2 = CreateDnsService(2);
  net::IPEndPoint ip_endpoint1 = CreateIPEndPoint(1);
  net::IPEndPoint ip_endpoint2 = CreateIPEndPoint(2);

  EXPECT_FALSE(mock_timer_->IsRunning());
  // finds service 1 & 2.
  DnsSdRegistry::DnsSdServiceList service_list1{service1, service2};
  media_sink_service_->OnDnsSdEvent(CastMediaSinkService::kCastServiceType,
                                    service_list1);

  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint1, _, _, _));
  EXPECT_CALL(*mock_cast_socket_service_,
              OpenSocketInternal(ip_endpoint2, _, _, _));
  base::RunLoop().RunUntilIdle();

  // Channel 2 is opened.
  media_sink_service_->OnChannelOpenedOnUIThread(service2, 2, false);

  std::vector<MediaSinkInternal> sinks;
  EXPECT_CALL(mock_sink_discovered_cb_, Run(_)).WillOnce(SaveArg<0>(&sinks));

  // Fire timer.
  mock_timer_->Fire();
  EXPECT_EQ(size_t(1), sinks.size());
  VerifyMediaSinkInternal(sinks[0], service2, 2, false);

  EXPECT_FALSE(mock_timer_->IsRunning());
  // Channel 1 is opened and timer is restarted.
  media_sink_service_->OnChannelOpenedOnUIThread(service1, 1, false);
  EXPECT_TRUE(mock_timer_->IsRunning());
}

}  // namespace media_router
