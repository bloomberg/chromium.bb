// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service_impl.h"
#include "base/test/mock_callback.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/media/router/discovery/dial/dial_registry.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace media_router {

class TestDialRegistry : public DialRegistry {
 public:
  TestDialRegistry() {}
  ~TestDialRegistry() {}

  MOCK_METHOD1(RegisterObserver, void(DialRegistry::Observer* observer));
  MOCK_METHOD1(UnregisterObserver, void(DialRegistry::Observer* observer));

  MOCK_METHOD0(OnListenerAdded, void());
  MOCK_METHOD0(OnListenerRemoved, void());
};

class MockDeviceDescriptionService : public DeviceDescriptionService {
 public:
  MockDeviceDescriptionService(DeviceDescriptionParseSuccessCallback success_cb,
                               DeviceDescriptionParseErrorCallback error_cb)
      : DeviceDescriptionService(/*connector=*/nullptr, success_cb, error_cb) {}
  ~MockDeviceDescriptionService() override {}

  MOCK_METHOD1(GetDeviceDescriptions,
               void(const std::vector<DialDeviceData>& devices));
};

class MockDialAppDiscoveryService : public DialAppDiscoveryService {
 public:
  MockDialAppDiscoveryService(
      const DialAppInfoParseCompletedCallback& completed_cb)
      : DialAppDiscoveryService(/*connector=*/nullptr, completed_cb) {}
  ~MockDialAppDiscoveryService() override {}

  MOCK_METHOD2(FetchDialAppInfo,
               void(const MediaSinkInternal& sink,
                    const std::string& app_name));
};

class DialMediaSinkServiceImplTest : public ::testing::Test {
 public:
  DialMediaSinkServiceImplTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        media_sink_service_(new DialMediaSinkServiceImpl(
            std::unique_ptr<service_manager::Connector>(),
            mock_sink_discovered_cb_.Get(),
            dial_sink_added_cb_.Get(),
            mock_available_sinks_updated_cb_.Get(),
            base::SequencedTaskRunnerHandle::Get())) {}

  void SetUp() override {
    media_sink_service_->SetDialRegistryForTest(&test_dial_registry_);

    auto mock_description_service =
        std::make_unique<MockDeviceDescriptionService>(mock_success_cb_.Get(),
                                                       mock_error_cb_.Get());
    mock_description_service_ = mock_description_service.get();
    media_sink_service_->SetDescriptionServiceForTest(
        std::move(mock_description_service));

    mock_timer_ =
        new base::MockTimer(true /*retain_user_task*/, false /*is_repeating*/);
    media_sink_service_->SetTimerForTest(base::WrapUnique(mock_timer_));

    auto mock_app_discovery_service =
        std::make_unique<MockDialAppDiscoveryService>(base::BindRepeating(
            &DialMediaSinkServiceImpl::OnAppInfoParseCompleted,
            base::Unretained(media_sink_service_.get())));
    mock_app_discovery_service_ = mock_app_discovery_service.get();
    media_sink_service_->SetAppDiscoveryServiceForTest(
        std::move(mock_app_discovery_service));
  }

 protected:
  const content::TestBrowserThreadBundle thread_bundle_;

  base::MockCallback<OnSinksDiscoveredCallback> mock_sink_discovered_cb_;
  base::MockCallback<OnDialSinkAddedCallback> dial_sink_added_cb_;
  base::MockCallback<OnAvailableSinksUpdatedCallback>
      mock_available_sinks_updated_cb_;
  base::MockCallback<
      MockDeviceDescriptionService::DeviceDescriptionParseSuccessCallback>
      mock_success_cb_;
  base::MockCallback<
      MockDeviceDescriptionService::DeviceDescriptionParseErrorCallback>
      mock_error_cb_;

  TestDialRegistry test_dial_registry_;
  MockDeviceDescriptionService* mock_description_service_;
  MockDialAppDiscoveryService* mock_app_discovery_service_;
  base::MockTimer* mock_timer_;

  std::unique_ptr<DialMediaSinkServiceImpl> media_sink_service_;

  DISALLOW_COPY_AND_ASSIGN(DialMediaSinkServiceImplTest);
};

TEST_F(DialMediaSinkServiceImplTest, TestOnDeviceDescriptionAvailable) {
  DialDeviceData device_data("first", GURL("http://127.0.0.1/dd.xml"),
                             base::Time::Now());
  ParsedDialDeviceDescription device_description;
  device_description.model_name = "model name";
  device_description.friendly_name = "friendly name";
  device_description.app_url = GURL("http://192.168.1.1/apps");
  device_description.unique_id = "unique id";

  media_sink_service_->OnDeviceDescriptionAvailable(device_data,
                                                    device_description);
  EXPECT_TRUE(media_sink_service_->current_sinks_.empty());

  std::vector<DialDeviceData> device_list = {device_data};
  EXPECT_CALL(*mock_description_service_, GetDeviceDescriptions(device_list));

  EXPECT_CALL(dial_sink_added_cb_, Run(_));
  media_sink_service_->OnDialDeviceEvent(device_list);
  media_sink_service_->OnDeviceDescriptionAvailable(device_data,
                                                    device_description);

  EXPECT_EQ(1u, media_sink_service_->current_sinks_.size());
}

TEST_F(DialMediaSinkServiceImplTest,
       TestOnDeviceDescriptionAvailableIPAddressChanged) {
  DialDeviceData device_data("first", GURL("http://127.0.0.1/dd.xml"),
                             base::Time::Now());
  ParsedDialDeviceDescription device_description;
  device_description.model_name = "model name";
  device_description.friendly_name = "friendly name";
  device_description.app_url = GURL("http://192.168.1.1/apps");
  device_description.unique_id = "unique id";

  std::vector<DialDeviceData> device_list = {device_data};
  EXPECT_CALL(*mock_description_service_, GetDeviceDescriptions(device_list));
  media_sink_service_->OnDialDeviceEvent(device_list);

  EXPECT_CALL(dial_sink_added_cb_, Run(_));
  media_sink_service_->OnDeviceDescriptionAvailable(device_data,
                                                    device_description);
  EXPECT_EQ(1u, media_sink_service_->current_sinks_.size());

  EXPECT_CALL(dial_sink_added_cb_, Run(_));
  device_description.app_url = GURL("http://192.168.1.100/apps");
  media_sink_service_->OnDeviceDescriptionAvailable(device_data,
                                                    device_description);

  EXPECT_EQ(1u, media_sink_service_->current_sinks_.size());
  for (const auto& dial_sink_it : media_sink_service_->current_sinks_) {
    EXPECT_EQ(device_description.app_url,
              dial_sink_it.second.dial_data().app_url);
  }
}

TEST_F(DialMediaSinkServiceImplTest, TestOnDeviceDescriptionRestartsTimer) {
  DialDeviceData device_data("first", GURL("http://127.0.0.1/dd.xml"),
                             base::Time::Now());
  ParsedDialDeviceDescription device_description;
  device_description.model_name = "model name";
  device_description.friendly_name = "friendly name";
  device_description.app_url = GURL("http://192.168.1.1/apps");
  device_description.unique_id = "unique id";

  std::vector<DialDeviceData> device_list = {device_data};
  EXPECT_CALL(*mock_description_service_, GetDeviceDescriptions(device_list));

  EXPECT_FALSE(mock_timer_->IsRunning());
  EXPECT_CALL(dial_sink_added_cb_, Run(_));
  media_sink_service_->OnDialDeviceEvent(device_list);
  media_sink_service_->OnDeviceDescriptionAvailable(device_data,
                                                    device_description);
  EXPECT_TRUE(mock_timer_->IsRunning());

  EXPECT_CALL(dial_sink_added_cb_, Run(_));
  EXPECT_CALL(mock_sink_discovered_cb_, Run(_));
  mock_timer_->Fire();

  EXPECT_FALSE(mock_timer_->IsRunning());
  device_description.app_url = GURL("http://192.168.1.11/apps");
  media_sink_service_->OnDeviceDescriptionAvailable(device_data,
                                                    device_description);
  EXPECT_TRUE(mock_timer_->IsRunning());
}

TEST_F(DialMediaSinkServiceImplTest, TestOnDialDeviceEventRestartsTimer) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  media_sink_service_->current_sinks_.insert_or_assign(dial_sink.sink().id(),
                                                       dial_sink);

  EXPECT_CALL(mock_sink_discovered_cb_, Run(_));
  media_sink_service_->OnDiscoveryComplete();

  EXPECT_FALSE(mock_timer_->IsRunning());
  EXPECT_CALL(*mock_description_service_,
              GetDeviceDescriptions(std::vector<DialDeviceData>()));
  media_sink_service_->OnDialDeviceEvent(std::vector<DialDeviceData>());
  EXPECT_TRUE(mock_timer_->IsRunning());

  EXPECT_CALL(mock_sink_discovered_cb_, Run(std::vector<MediaSinkInternal>()));
  mock_timer_->Fire();

  EXPECT_CALL(*mock_description_service_,
              GetDeviceDescriptions(std::vector<DialDeviceData>()));
  media_sink_service_->OnDialDeviceEvent(std::vector<DialDeviceData>());
  EXPECT_TRUE(mock_timer_->IsRunning());

  EXPECT_CALL(mock_sink_discovered_cb_, Run(_)).Times(0);
  mock_timer_->Fire();
}

TEST_F(DialMediaSinkServiceImplTest, OnDialSinkAddedCallback) {
  DialDeviceData device_data1("first", GURL("http://127.0.0.1/dd.xml"),
                              base::Time::Now());
  ParsedDialDeviceDescription device_description1;
  device_description1.model_name = "model name";
  device_description1.friendly_name = "friendly name";
  device_description1.app_url = GURL("http://192.168.1.1/apps");
  device_description1.unique_id = "unique id 1";

  DialDeviceData device_data2("second", GURL("http://127.0.0.2/dd.xml"),
                              base::Time::Now());
  ParsedDialDeviceDescription device_description2;
  device_description2.model_name = "model name";
  device_description2.friendly_name = "friendly name";
  device_description2.app_url = GURL("http://192.168.1.2/apps");
  device_description2.unique_id = "unique id 2";

  std::vector<DialDeviceData> device_list = {device_data1, device_data2};
  EXPECT_CALL(*mock_description_service_, GetDeviceDescriptions(device_list));
  media_sink_service_->OnDialDeviceEvent(device_list);

  EXPECT_CALL(dial_sink_added_cb_, Run(_));
  media_sink_service_->OnDeviceDescriptionAvailable(device_data1,
                                                    device_description1);

  EXPECT_CALL(dial_sink_added_cb_, Run(_));
  media_sink_service_->OnDeviceDescriptionAvailable(device_data2,
                                                    device_description2);

  // OnUserGesture will "re-sync" all existing sinks to callback.
  EXPECT_CALL(dial_sink_added_cb_, Run(_)).Times(2);
  media_sink_service_->OnUserGesture();

}

TEST_F(DialMediaSinkServiceImplTest,
       TestStartStopMonitoringAvailableSinksForApp) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  EXPECT_CALL(*mock_app_discovery_service_,
              FetchDialAppInfo(dial_sink, "YouTube"));
  EXPECT_CALL(mock_available_sinks_updated_cb_,
              Run("YouTube", std::vector<MediaSinkInternal>()));
  media_sink_service_->current_sinks_.insert_or_assign(dial_sink.sink().id(),
                                                       dial_sink);
  media_sink_service_->StartMonitoringAvailableSinksForApp("YouTube");
  media_sink_service_->StartMonitoringAvailableSinksForApp("YouTube");
  EXPECT_EQ(1u, media_sink_service_->registered_apps_.size());

  media_sink_service_->StopMonitoringAvailableSinksForApp("YouTube");
  EXPECT_TRUE(media_sink_service_->registered_apps_.empty());
}

TEST_F(DialMediaSinkServiceImplTest,
       TestOnDialAppInfoAvailableNoStartMonitoring) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  std::string sink_id = dial_sink.sink().id();

  EXPECT_CALL(mock_available_sinks_updated_cb_, Run(_, _)).Times(0);
  media_sink_service_->current_sinks_.insert_or_assign(sink_id, dial_sink);
  media_sink_service_->OnAppInfoParseCompleted(sink_id, "YouTube",
                                               SinkAppStatus::kAvailable);
}

TEST_F(DialMediaSinkServiceImplTest, TestOnDialAppInfoAvailableNoSink) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  std::string sink_id = dial_sink.sink().id();

  EXPECT_CALL(mock_available_sinks_updated_cb_,
              Run("YouTube", std::vector<MediaSinkInternal>()))
      .Times(2);
  media_sink_service_->StartMonitoringAvailableSinksForApp("YouTube");
  media_sink_service_->OnAppInfoParseCompleted(sink_id, "YouTube",
                                               SinkAppStatus::kAvailable);
}

TEST_F(DialMediaSinkServiceImplTest, TestOnDialAppInfoAvailableSinksAdded) {
  MediaSinkInternal dial_sink1 = CreateDialSink(1);
  MediaSinkInternal dial_sink2 = CreateDialSink(2);
  std::string sink_id1 = dial_sink1.sink().id();
  std::string sink_id2 = dial_sink2.sink().id();

  media_sink_service_->current_sinks_.insert_or_assign(sink_id1, dial_sink1);
  media_sink_service_->current_sinks_.insert_or_assign(sink_id2, dial_sink2);

  EXPECT_CALL(*mock_app_discovery_service_, FetchDialAppInfo(_, _)).Times(4);
  EXPECT_CALL(mock_available_sinks_updated_cb_,
              Run("YouTube", std::vector<MediaSinkInternal>()));
  EXPECT_CALL(mock_available_sinks_updated_cb_,
              Run("Netflix", std::vector<MediaSinkInternal>()));
  media_sink_service_->StartMonitoringAvailableSinksForApp("YouTube");
  media_sink_service_->StartMonitoringAvailableSinksForApp("Netflix");

  EXPECT_CALL(mock_available_sinks_updated_cb_,
              Run("YouTube", std::vector<MediaSinkInternal>({dial_sink1})));
  media_sink_service_->OnAppInfoParseCompleted(sink_id1, "YouTube",
                                               SinkAppStatus::kAvailable);

  EXPECT_CALL(
      mock_available_sinks_updated_cb_,
      Run("YouTube", std::vector<MediaSinkInternal>({dial_sink1, dial_sink2})));
  media_sink_service_->OnAppInfoParseCompleted(sink_id2, "YouTube",
                                               SinkAppStatus::kAvailable);

  EXPECT_CALL(mock_available_sinks_updated_cb_,
              Run("Netflix", std::vector<MediaSinkInternal>({dial_sink2})));
  media_sink_service_->OnAppInfoParseCompleted(sink_id2, "Netflix",
                                               SinkAppStatus::kAvailable);
}

TEST_F(DialMediaSinkServiceImplTest, TestOnDialAppInfoAvailableSinksRemoved) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  std::string sink_id = dial_sink.sink().id();

  EXPECT_CALL(*mock_app_discovery_service_, FetchDialAppInfo(_, _));
  EXPECT_CALL(mock_available_sinks_updated_cb_, Run(_, _));
  media_sink_service_->current_sinks_.insert_or_assign(sink_id, dial_sink);
  media_sink_service_->StartMonitoringAvailableSinksForApp("YouTube");

  EXPECT_CALL(mock_available_sinks_updated_cb_,
              Run("YouTube", std::vector<MediaSinkInternal>({dial_sink})));
  media_sink_service_->OnAppInfoParseCompleted(sink_id, "YouTube",
                                               SinkAppStatus::kAvailable);

  EXPECT_CALL(mock_available_sinks_updated_cb_,
              Run("YouTube", std::vector<MediaSinkInternal>()));
  media_sink_service_->OnAppInfoParseCompleted(sink_id, "YouTube",
                                               SinkAppStatus::kUnavailable);
}

TEST_F(DialMediaSinkServiceImplTest,
       TestOnDialAppInfoAvailableWithAlreadyAvailableSinks) {
  MediaSinkInternal dial_sink = CreateDialSink(1);

  EXPECT_CALL(*mock_app_discovery_service_, FetchDialAppInfo(_, _));
  EXPECT_CALL(mock_available_sinks_updated_cb_, Run(_, _));
  media_sink_service_->current_sinks_.insert_or_assign(dial_sink.sink().id(),
                                                       dial_sink);
  media_sink_service_->StartMonitoringAvailableSinksForApp("YouTube");

  EXPECT_CALL(mock_available_sinks_updated_cb_,
              Run("YouTube", std::vector<MediaSinkInternal>({dial_sink})))
      .Times(1);
  media_sink_service_->OnAppInfoParseCompleted(dial_sink.sink().id(), "YouTube",
                                               SinkAppStatus::kAvailable);
  media_sink_service_->OnAppInfoParseCompleted(dial_sink.sink().id(), "YouTube",
                                               SinkAppStatus::kAvailable);
}

TEST_F(DialMediaSinkServiceImplTest, TestFetchDialAppInfoWithCachedAppInfo) {
  MediaSinkInternal dial_sink = CreateDialSink(1);

  EXPECT_CALL(*mock_app_discovery_service_, FetchDialAppInfo(_, _));
  EXPECT_CALL(mock_available_sinks_updated_cb_, Run(_, _));
  media_sink_service_->current_sinks_.insert_or_assign(dial_sink.sink().id(),
                                                       dial_sink);
  media_sink_service_->StartMonitoringAvailableSinksForApp("YouTube");

  EXPECT_CALL(mock_available_sinks_updated_cb_,
              Run("YouTube", std::vector<MediaSinkInternal>({dial_sink})))
      .Times(1);
  media_sink_service_->OnAppInfoParseCompleted(dial_sink.sink().id(), "YouTube",
                                               SinkAppStatus::kAvailable);

  EXPECT_CALL(*mock_app_discovery_service_, FetchDialAppInfo(_, _)).Times(0);
  media_sink_service_->StartMonitoringAvailableSinksForApp("YouTube");
}

TEST_F(DialMediaSinkServiceImplTest, TestStartAfterStopMonitoringForApp) {
  MediaSinkInternal dial_sink = CreateDialSink(1);

  EXPECT_CALL(*mock_app_discovery_service_, FetchDialAppInfo(_, _));
  EXPECT_CALL(mock_available_sinks_updated_cb_, Run(_, _));
  media_sink_service_->current_sinks_.insert_or_assign(dial_sink.sink().id(),
                                                       dial_sink);
  media_sink_service_->StartMonitoringAvailableSinksForApp("YouTube");

  EXPECT_CALL(mock_available_sinks_updated_cb_,
              Run("YouTube", std::vector<MediaSinkInternal>({dial_sink})));
  media_sink_service_->OnAppInfoParseCompleted(dial_sink.sink().id(), "YouTube",
                                               SinkAppStatus::kAvailable);

  media_sink_service_->StopMonitoringAvailableSinksForApp("YouTube");

  EXPECT_CALL(mock_available_sinks_updated_cb_,
              Run("YouTube", std::vector<MediaSinkInternal>({dial_sink})));
  media_sink_service_->StartMonitoringAvailableSinksForApp("YouTube");
}

TEST_F(DialMediaSinkServiceImplTest,
       TestFetchDialAppInfoWithDiscoveryOnlySink) {
  MediaSinkInternal dial_sink = CreateDialSink(1);
  media_router::DialSinkExtraData extra_data = dial_sink.dial_data();
  extra_data.model_name = "Eureka Dongle";
  dial_sink.set_dial_data(extra_data);

  EXPECT_CALL(*mock_app_discovery_service_, FetchDialAppInfo(_, _)).Times(0);
  EXPECT_CALL(mock_available_sinks_updated_cb_, Run(_, _));
  media_sink_service_->current_sinks_.insert_or_assign(dial_sink.sink().id(),
                                                       dial_sink);
  media_sink_service_->StartMonitoringAvailableSinksForApp("YouTube");
}

}  // namespace media_router
