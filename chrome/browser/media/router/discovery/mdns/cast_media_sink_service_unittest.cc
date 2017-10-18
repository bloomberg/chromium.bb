// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/mock_dns_sd_registry.h"
#include "chrome/browser/media/router/test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/cast_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

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
  service.service_host_port = net::HostPortPair::FromIPEndPoint(ip_endpoint);
  service.service_data.push_back(base::StringPrintf("id=service %d", num));
  service.service_data.push_back(
      base::StringPrintf("fn=friendly name %d", num));
  service.service_data.push_back(base::StringPrintf("md=model name %d", num));

  return service;
}

}  // namespace

namespace media_router {

class MockCastMediaSinkServiceImpl : public CastMediaSinkServiceImpl {
 public:
  MockCastMediaSinkServiceImpl(
      const OnSinksDiscoveredCallback& callback,
      cast_channel::CastSocketService* cast_socket_service,
      DiscoveryNetworkMonitor* network_monitor,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
      : CastMediaSinkServiceImpl(callback,
                                 cast_socket_service,
                                 network_monitor,
                                 nullptr /* url_request_context_getter */,
                                 task_runner) {}
  ~MockCastMediaSinkServiceImpl() override {}

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD2(OpenChannels,
               void(const std::vector<MediaSinkInternal>& cast_sinks,
                    CastMediaSinkServiceImpl::SinkSource sink_source));
};

class CastMediaSinkServiceTest : public ::testing::Test {
 public:
  CastMediaSinkServiceTest()
      : mock_cast_socket_service_(new cast_channel::MockCastSocketService()),
        task_runner_(new base::TestSimpleTaskRunner()),
        mock_media_sink_service_impl_(
            new MockCastMediaSinkServiceImpl(mock_sink_discovered_io_cb_.Get(),
                                             mock_cast_socket_service_.get(),
                                             discovery_network_monitor_,
                                             task_runner_)),
        media_sink_service_(new CastMediaSinkService(
            mock_sink_discovered_ui_cb_.Get(),
            task_runner_,
            std::unique_ptr<CastMediaSinkServiceImpl,
                            content::BrowserThread::DeleteOnIOThread>(
                mock_media_sink_service_impl_))),
        test_dns_sd_registry_(media_sink_service_.get()) {}

 protected:
  const content::TestBrowserThreadBundle thread_bundle_;

  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_ =
      base::WrapUnique(net::NetworkChangeNotifier::CreateMock());
  DiscoveryNetworkMonitor* const discovery_network_monitor_ =
      DiscoveryNetworkMonitor::GetInstance();

  TestingProfile profile_;

  base::MockCallback<MediaSinkService::OnSinksDiscoveredCallback>
      mock_sink_discovered_io_cb_;
  base::MockCallback<MediaSinkService::OnSinksDiscoveredCallback>
      mock_sink_discovered_ui_cb_;
  std::unique_ptr<cast_channel::MockCastSocketService>
      mock_cast_socket_service_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  MockCastMediaSinkServiceImpl* mock_media_sink_service_impl_;

  scoped_refptr<CastMediaSinkService> media_sink_service_;
  MockDnsSdRegistry test_dns_sd_registry_;

  DISALLOW_COPY_AND_ASSIGN(CastMediaSinkServiceTest);
};

TEST_F(CastMediaSinkServiceTest, TestResetBeforeStop) {
  EXPECT_CALL(test_dns_sd_registry_, AddObserver(media_sink_service_.get()));
  EXPECT_CALL(test_dns_sd_registry_, RegisterDnsSdListener(_));

  media_sink_service_->SetDnsSdRegistryForTest(&test_dns_sd_registry_);
  media_sink_service_->Start();

  EXPECT_CALL(test_dns_sd_registry_, RemoveObserver(media_sink_service_.get()));
  EXPECT_CALL(test_dns_sd_registry_, UnregisterDnsSdListener(_));
  media_sink_service_->Stop();

  media_sink_service_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

TEST_F(CastMediaSinkServiceTest, TestRestartAfterStop) {
  EXPECT_CALL(test_dns_sd_registry_, AddObserver(media_sink_service_.get()))
      .Times(2);
  EXPECT_CALL(test_dns_sd_registry_, RegisterDnsSdListener(_));
  media_sink_service_->SetDnsSdRegistryForTest(&test_dns_sd_registry_);
  media_sink_service_->Start();

  EXPECT_CALL(test_dns_sd_registry_, RemoveObserver(media_sink_service_.get()));
  EXPECT_CALL(test_dns_sd_registry_, UnregisterDnsSdListener(_));
  media_sink_service_->Stop();

  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(test_dns_sd_registry_, RegisterDnsSdListener(_));
  media_sink_service_->SetDnsSdRegistryForTest(&test_dns_sd_registry_);
  media_sink_service_->Start();
}

TEST_F(CastMediaSinkServiceTest, TestMultipleStartAndStop) {
  EXPECT_CALL(test_dns_sd_registry_, AddObserver(media_sink_service_.get()));
  EXPECT_CALL(test_dns_sd_registry_, RegisterDnsSdListener(_));
  media_sink_service_->SetDnsSdRegistryForTest(&test_dns_sd_registry_);
  media_sink_service_->Start();
  media_sink_service_->Start();

  EXPECT_CALL(test_dns_sd_registry_, RemoveObserver(media_sink_service_.get()));
  EXPECT_CALL(test_dns_sd_registry_, UnregisterDnsSdListener(_));
  media_sink_service_->Stop();
  media_sink_service_->Stop();

  base::RunLoop().RunUntilIdle();
}

TEST_F(CastMediaSinkServiceTest, OnUserGesture) {
  EXPECT_CALL(test_dns_sd_registry_, ForceDiscovery()).Times(0);
  media_sink_service_->OnUserGesture();

  EXPECT_CALL(test_dns_sd_registry_, AddObserver(media_sink_service_.get()));
  EXPECT_CALL(test_dns_sd_registry_, RegisterDnsSdListener(_));
  EXPECT_CALL(test_dns_sd_registry_, ForceDiscovery());
  media_sink_service_->SetDnsSdRegistryForTest(&test_dns_sd_registry_);
  media_sink_service_->OnUserGesture();
}

TEST_F(CastMediaSinkServiceTest, TestOnDnsSdEvent) {
  DnsSdService service1 = CreateDnsService(1);
  DnsSdService service2 = CreateDnsService(2);

  // Add dns services.
  DnsSdRegistry::DnsSdServiceList service_list{service1, service2};

  // Invoke CastSocketService::OpenSocket on the IO thread.
  media_sink_service_->OnDnsSdEvent(CastMediaSinkService::kCastServiceType,
                                    service_list);

  std::vector<MediaSinkInternal> sinks;
  EXPECT_CALL(*mock_media_sink_service_impl_, OpenChannels(_, _))
      .WillOnce(SaveArg<0>(&sinks));

  // Invoke OpenChannels on the IO thread.
  task_runner_->RunUntilIdle();
  // Verify sink content
  EXPECT_EQ(2u, sinks.size());

  // Invoke callback on the UI thread.
  EXPECT_CALL(mock_sink_discovered_ui_cb_, Run(_));

  media_sink_service_->OnSinksDiscoveredOnIOThread(sinks);
  base::RunLoop().RunUntilIdle();
}

TEST_F(CastMediaSinkServiceTest, TestForceSinkDiscoveryCallback) {
  EXPECT_CALL(mock_sink_discovered_io_cb_, Run(_));

  media_sink_service_->ForceSinkDiscoveryCallback();
  task_runner_->RunUntilIdle();
}

}  // namespace media_router
