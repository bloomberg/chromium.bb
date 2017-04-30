// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_media_sink_service.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/media/router/discovery/dial/dial_registry.h"
#include "chrome/browser/media/router/test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace {

media_router::DialSinkExtraData CreateDialSinkExtraData(
    const std::string& model_name,
    const std::string& ip_address,
    const std::string& app_url) {
  media_router::DialSinkExtraData dial_extra_data;
  EXPECT_TRUE(dial_extra_data.ip_address.AssignFromIPLiteral(ip_address));
  dial_extra_data.model_name = model_name;
  dial_extra_data.app_url = GURL(app_url);
  return dial_extra_data;
}

std::vector<media_router::MediaSinkInternal> CreateDialMediaSinks() {
  media_router::MediaSink sink1("sink1", "sink_name_1",
                                media_router::MediaSink::IconType::CAST);
  media_router::DialSinkExtraData extra_data1 = CreateDialSinkExtraData(
      "model_name1", "192.168.1.1", "https://example1.com");

  media_router::MediaSink sink2("sink2", "sink_name_2",
                                media_router::MediaSink::IconType::CAST);
  media_router::DialSinkExtraData extra_data2 = CreateDialSinkExtraData(
      "model_name2", "192.168.1.2", "https://example2.com");

  std::vector<media_router::MediaSinkInternal> sinks;
  sinks.push_back(media_router::MediaSinkInternal(sink1, extra_data1));
  sinks.push_back(media_router::MediaSinkInternal(sink2, extra_data2));
  return sinks;
}

}  // namespace

namespace media_router {

class TestDialRegistry : public DialRegistry {
 public:
  TestDialRegistry() {}
  ~TestDialRegistry() {}

  MOCK_METHOD1(RegisterObserver, void(DialRegistry::Observer* observer));
  MOCK_METHOD1(UnregisterObserver, void(DialRegistry::Observer* observer));
};

class MockDeviceDescriptionService : public DeviceDescriptionService {
 public:
  MockDeviceDescriptionService(DeviceDescriptionParseSuccessCallback success_cb,
                               DeviceDescriptionParseErrorCallback error_cb)
      : DeviceDescriptionService(success_cb, error_cb) {}
  ~MockDeviceDescriptionService() override {}

  MOCK_METHOD2(GetDeviceDescriptions,
               void(const std::vector<DialDeviceData>& devices,
                    net::URLRequestContextGetter* request_context));
};

class TestDialMediaSinkService : public DialMediaSinkService {
 public:
  TestDialMediaSinkService(
      const MediaSinkService::OnSinksDiscoveredCallback& callback,
      net::URLRequestContextGetter* request_context,
      TestDialRegistry* test_dial_registry,
      MockDeviceDescriptionService* mock_description_service)
      : DialMediaSinkService(callback, request_context),
        test_dial_registry_(test_dial_registry),
        mock_description_service_(mock_description_service) {}

  DialRegistry* dial_registry() override { return test_dial_registry_; }

  DeviceDescriptionService* GetDescriptionService() override {
    return mock_description_service_;
  }

 private:
  TestDialRegistry* test_dial_registry_;
  MockDeviceDescriptionService* mock_description_service_;
};

class DialMediaSinkServiceTest : public ::testing::Test {
 public:
  DialMediaSinkServiceTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        mock_description_service_(mock_success_cb_.Get(), mock_error_cb_.Get()),
        media_sink_service_(mock_sink_discovered_cb_.Get(),
                            profile_.GetRequestContext(),
                            &test_dial_registry_,
                            &mock_description_service_) {}

  DialMediaSinkService* media_sink_service() { return &media_sink_service_; }

  void TestFetchCompleted(const std::vector<MediaSinkInternal>& old_sinks,
                          const std::vector<MediaSinkInternal>& new_sinks) {
    media_sink_service()->mrp_sinks_ =
        std::set<MediaSinkInternal>(old_sinks.begin(), old_sinks.end());
    media_sink_service()->current_sinks_ =
        std::set<MediaSinkInternal>(new_sinks.begin(), new_sinks.end());
    EXPECT_CALL(mock_sink_discovered_cb_, Run(new_sinks));
    media_sink_service()->OnFetchCompleted();
  }

 protected:
  const content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;

  base::MockCallback<MediaSinkService::OnSinksDiscoveredCallback>
      mock_sink_discovered_cb_;
  base::MockCallback<
      MockDeviceDescriptionService::DeviceDescriptionParseSuccessCallback>
      mock_success_cb_;
  base::MockCallback<
      MockDeviceDescriptionService::DeviceDescriptionParseErrorCallback>
      mock_error_cb_;

  TestDialRegistry test_dial_registry_;
  MockDeviceDescriptionService mock_description_service_;

  TestDialMediaSinkService media_sink_service_;
};

TEST_F(DialMediaSinkServiceTest, TestStart) {
  EXPECT_CALL(test_dial_registry_, RegisterObserver(&media_sink_service_));
  media_sink_service()->Start();

  DialRegistry::DeviceList deviceList;
  DialDeviceData first_device("first", GURL("http://127.0.0.1/dd.xml"),
                              base::Time::Now());
  DialDeviceData second_device("second", GURL("http://127.0.0.2/dd.xml"),
                               base::Time::Now());
  DialDeviceData third_device("third", GURL("http://127.0.0.3/dd.xml"),
                              base::Time::Now());
  deviceList.push_back(first_device);
  deviceList.push_back(second_device);
  deviceList.push_back(third_device);

  EXPECT_CALL(mock_description_service_, GetDeviceDescriptions(deviceList, _));

  media_sink_service()->OnDialDeviceEvent(deviceList);
  EXPECT_TRUE(media_sink_service_.finish_timer_->IsRunning());
}

TEST_F(DialMediaSinkServiceTest, TestOnDeviceDescriptionAvailable) {
  DialDeviceData device_data("first", GURL("http://127.0.0.1/dd.xml"),
                             base::Time::Now());
  ParsedDialDeviceDescription device_description;
  device_description.model_name = "model name";
  device_description.friendly_name = "friendly name";
  device_description.app_url = GURL("http://192.168.1.1/apps");
  device_description.unique_id = "unique id";

  media_sink_service()->OnDeviceDescriptionAvailable(device_data,
                                                     device_description);
  EXPECT_TRUE(media_sink_service()->current_sinks_.empty());

  std::vector<DialDeviceData> deviceList{device_data};
  EXPECT_CALL(mock_description_service_, GetDeviceDescriptions(deviceList, _));

  media_sink_service()->OnDialDeviceEvent(deviceList);
  media_sink_service()->OnDeviceDescriptionAvailable(device_data,
                                                     device_description);

  EXPECT_EQ(size_t(1), media_sink_service()->current_sinks_.size());
}

TEST_F(DialMediaSinkServiceTest, TestFetchCompleted) {
  std::vector<MediaSinkInternal> old_sinks;
  std::vector<MediaSinkInternal> new_sinks = CreateDialMediaSinks();
  TestFetchCompleted(old_sinks, new_sinks);

  // Same sink
  EXPECT_CALL(mock_sink_discovered_cb_, Run(new_sinks)).Times(0);
  media_sink_service()->OnFetchCompleted();
}

TEST_F(DialMediaSinkServiceTest, TestFetchCompleted_OneNewSink) {
  std::vector<MediaSinkInternal> old_sinks = CreateDialMediaSinks();
  std::vector<MediaSinkInternal> new_sinks = CreateDialMediaSinks();
  MediaSink sink3("sink3", "sink_name_3", MediaSink::IconType::CAST);
  DialSinkExtraData extra_data3 = CreateDialSinkExtraData(
      "model_name3", "192.168.1.3", "https://example3.com");
  new_sinks.push_back(MediaSinkInternal(sink3, extra_data3));
  TestFetchCompleted(old_sinks, new_sinks);
}

TEST_F(DialMediaSinkServiceTest, TestFetchCompleted_RemovedOneSink) {
  std::vector<MediaSinkInternal> old_sinks = CreateDialMediaSinks();
  std::vector<MediaSinkInternal> new_sinks = CreateDialMediaSinks();
  new_sinks.erase(new_sinks.begin());
  TestFetchCompleted(old_sinks, new_sinks);
}

TEST_F(DialMediaSinkServiceTest, TestFetchCompleted_UpdatedOneSink) {
  std::vector<MediaSinkInternal> old_sinks = CreateDialMediaSinks();
  std::vector<MediaSinkInternal> new_sinks = CreateDialMediaSinks();
  new_sinks[0].set_name("sink_name_4");
  TestFetchCompleted(old_sinks, new_sinks);
}

TEST_F(DialMediaSinkServiceTest, TestFetchCompleted_Mixed) {
  std::vector<MediaSinkInternal> old_sinks = CreateDialMediaSinks();

  MediaSink sink1("sink1", "sink_name_1", MediaSink::IconType::CAST);
  DialSinkExtraData extra_data2 = CreateDialSinkExtraData(
      "model_name2", "192.168.1.2", "https://example2.com");

  MediaSink sink3("sink3", "sink_name_3", MediaSink::IconType::CAST);
  DialSinkExtraData extra_data3 = CreateDialSinkExtraData(
      "model_name3", "192.168.1.3", "https://example3.com");

  std::vector<MediaSinkInternal> new_sinks;
  new_sinks.push_back(MediaSinkInternal(sink1, extra_data2));
  new_sinks.push_back(MediaSinkInternal(sink3, extra_data3));

  TestFetchCompleted(old_sinks, new_sinks);
}

}  // namespace media_router
