// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/device_description_service.h"

#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/router/discovery/dial/device_description_fetcher.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/media/router/discovery/dial/parsed_dial_device_description.h"
#include "chrome/browser/media/router/discovery/dial/safe_dial_device_description_parser.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

// Create Test Data
namespace {

media_router::DialDeviceData CreateDialDeviceData(int num) {
  media_router::DialDeviceData device_data(
      base::StringPrintf("Device id %d", num),
      GURL(base::StringPrintf("http://192.168.1.%d/dd.xml", num)),
      base::Time::Now());
  device_data.set_label(base::StringPrintf("Device label %d", num));
  return device_data;
}

media_router::DialDeviceDescriptionData CreateDialDeviceDescriptionData(
    int num) {
  return media_router::DialDeviceDescriptionData(
      "", GURL(base::StringPrintf("http://192.168.1.%d/apps", num)));
}

chrome::mojom::DialDeviceDescriptionPtr CreateDialDeviceDescriptionPtr(
    int num) {
  chrome::mojom::DialDeviceDescriptionPtr description_ptr =
      chrome::mojom::DialDeviceDescription::New();
  description_ptr->friendly_name = base::StringPrintf("Friendly name %d", num);
  description_ptr->model_name = base::StringPrintf("Model name %d", num);
  description_ptr->unique_id = base::StringPrintf("Unique ID %d", num);
  return description_ptr;
}

media_router::ParsedDialDeviceDescription CreateParsedDialDeviceDescription(
    int num) {
  media_router::ParsedDialDeviceDescription description_data;
  description_data.app_url =
      GURL(base::StringPrintf("http://192.168.1.%d/apps", num));
  description_data.friendly_name = base::StringPrintf("Friendly name %d", num);
  description_data.model_name = base::StringPrintf("Model name %d", num);
  description_data.unique_id = base::StringPrintf("Unique ID %d", num);
  return description_data;
}

}  // namespace

namespace media_router {

class TestSafeDialDeviceDescriptionParser
    : public SafeDialDeviceDescriptionParser {
 public:
  ~TestSafeDialDeviceDescriptionParser() override {}

  MOCK_METHOD2(Start,
               void(const std::string& xml_text,
                    const DeviceDescriptionCallback& callback));
};

class TestDeviceDescriptionService : public DeviceDescriptionService {
 public:
  TestDeviceDescriptionService(
      const DeviceDescriptionParseSuccessCallback& success_cb,
      const DeviceDescriptionParseErrorCallback& error_cb)
      : DeviceDescriptionService(success_cb, error_cb) {}

  MOCK_METHOD0(CreateSafeParser, SafeDialDeviceDescriptionParser*());
};

class DeviceDescriptionServiceTest : public ::testing::Test {
 public:
  DeviceDescriptionServiceTest()
      : device_description_service_(mock_success_cb_.Get(),
                                    mock_error_cb_.Get()),
        fetcher_map_(
            device_description_service_.device_description_fetcher_map_),
        description_cache_(device_description_service_.description_cache_),
        thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

  TestDeviceDescriptionService* device_description_service() {
    return &device_description_service_;
  }

  void AddToCache(const std::string& device_label,
                  const ParsedDialDeviceDescription& description_data,
                  bool expired) {
    DeviceDescriptionService::CacheEntry cache_entry;
    cache_entry.expire_time =
        base::Time::Now() + (expired ? -1 : 1) * base::TimeDelta::FromHours(12);
    cache_entry.description_data = description_data;
    description_cache_[device_label] = cache_entry;
  }

  void SetTestParser(
      std::unique_ptr<TestSafeDialDeviceDescriptionParser> test_parser) {
    if (device_description_service_.parser_)
      return;
    device_description_service_.parser_ = std::move(test_parser);
  }

  void OnDeviceDescriptionFetchComplete(int num) {
    auto device_data = CreateDialDeviceData(num);
    auto description_response_data = CreateDialDeviceDescriptionData(num);
    auto parsed_description_data = CreateParsedDialDeviceDescription(num);

    EXPECT_CALL(mock_success_cb_, Run(device_data, parsed_description_data));

    device_description_service_.OnDeviceDescriptionFetchComplete(
        device_data, description_response_data);
    device_description_service_.OnParsedDeviceDescription(
        device_data, description_response_data.app_url,
        CreateDialDeviceDescriptionPtr(num),
        chrome::mojom::DialParsingError::NONE);
  }

  void TestOnParsedDeviceDescription(
      chrome::mojom::DialDeviceDescriptionPtr description_ptr,
      const std::string& error_message) {
    GURL app_url("http://192.168.1.1/apps");
    auto device_data = CreateDialDeviceData(1);
    auto description_data = CreateParsedDialDeviceDescription(1);

    if (!error_message.empty()) {
      EXPECT_CALL(mock_error_cb_, Run(device_data, error_message));
    } else {
      EXPECT_CALL(mock_success_cb_, Run(device_data, description_data));
    }
    device_description_service()->OnParsedDeviceDescription(
        device_data, app_url, std::move(description_ptr),
        chrome::mojom::DialParsingError::NONE);
  }

 protected:
  base::MockCallback<
      DeviceDescriptionService::DeviceDescriptionParseSuccessCallback>
      mock_success_cb_;
  base::MockCallback<
      DeviceDescriptionService::DeviceDescriptionParseErrorCallback>
      mock_error_cb_;

  TestDeviceDescriptionService device_description_service_;
  std::map<std::string, std::unique_ptr<DeviceDescriptionFetcher>>&
      fetcher_map_;
  std::map<std::string, DeviceDescriptionService::CacheEntry>&
      description_cache_;

  const content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

TEST_F(DeviceDescriptionServiceTest, TestGetDeviceDescriptionFromCache) {
  auto device_data = CreateDialDeviceData(1);
  auto description_data = CreateParsedDialDeviceDescription(1);
  EXPECT_CALL(mock_success_cb_, Run(device_data, description_data));

  AddToCache(device_data.label(), description_data, false /* expired */);

  std::vector<DialDeviceData> devices = {device_data};
  device_description_service()->GetDeviceDescriptions(devices, nullptr);
}

TEST_F(DeviceDescriptionServiceTest, TestGetDeviceDescriptionFetchURL) {
  DialDeviceData device_data = CreateDialDeviceData(1);
  std::vector<DialDeviceData> devices = {device_data};

  // Create Fetcher
  EXPECT_TRUE(fetcher_map_.empty());
  device_description_service()->GetDeviceDescriptions(
      devices, profile_.GetRequestContext());
  EXPECT_EQ(size_t(1), fetcher_map_.size());

  // Remove fetcher and create safe parser
  auto test_parser = base::MakeUnique<TestSafeDialDeviceDescriptionParser>();
  EXPECT_CALL(*test_parser, Start(_, _));

  SetTestParser(std::move(test_parser));
  OnDeviceDescriptionFetchComplete(1);
}

TEST_F(DeviceDescriptionServiceTest, TestGetDeviceDescriptionFetchURLError) {
  DialDeviceData device_data = CreateDialDeviceData(1);
  std::vector<DialDeviceData> devices;
  devices.push_back(device_data);

  // Create Fetcher
  EXPECT_TRUE(fetcher_map_.empty());
  device_description_service()->GetDeviceDescriptions(
      devices, profile_.GetRequestContext());
  EXPECT_EQ(size_t(1), fetcher_map_.size());

  EXPECT_CALL(mock_error_cb_, Run(device_data, ""));

  device_description_service()->OnDeviceDescriptionFetchError(device_data, "");
  EXPECT_TRUE(fetcher_map_.empty());
}

TEST_F(DeviceDescriptionServiceTest,
       TestGetDeviceDescriptionRemoveOutDatedFetchers) {
  DialDeviceData device_data_1 = CreateDialDeviceData(1);
  DialDeviceData device_data_2 = CreateDialDeviceData(2);
  DialDeviceData device_data_3 = CreateDialDeviceData(3);

  std::vector<DialDeviceData> devices;
  devices.push_back(device_data_1);
  devices.push_back(device_data_2);

  // insert fetchers
  device_description_service()->GetDeviceDescriptions(
      devices, profile_.GetRequestContext());

  // Keep fetchers non exist in current device list and remove fetchers with
  // different description url.
  GURL new_url_2 = GURL("http://example.com");
  device_data_2.set_device_description_url(new_url_2);

  devices.clear();
  devices.push_back(device_data_2);
  devices.push_back(device_data_3);
  device_description_service()->GetDeviceDescriptions(
      devices, profile_.GetRequestContext());

  EXPECT_EQ(size_t(3), fetcher_map_.size());

  auto* description_fetcher = fetcher_map_[device_data_2.label()].get();
  EXPECT_EQ(new_url_2, description_fetcher->device_description_url());

  EXPECT_CALL(mock_error_cb_, Run(_, _)).Times(3);
  device_description_service()->OnDeviceDescriptionFetchError(device_data_1,
                                                              "");
  device_description_service()->OnDeviceDescriptionFetchError(device_data_2,
                                                              "");
  device_description_service()->OnDeviceDescriptionFetchError(device_data_3,
                                                              "");
}

TEST_F(DeviceDescriptionServiceTest, TestCleanUpCacheEntries) {
  DialDeviceData device_data_1 = CreateDialDeviceData(1);
  DialDeviceData device_data_2 = CreateDialDeviceData(2);
  DialDeviceData device_data_3 = CreateDialDeviceData(3);

  AddToCache(device_data_1.label(), ParsedDialDeviceDescription(),
             true /* expired */);
  AddToCache(device_data_2.label(), ParsedDialDeviceDescription(),
             true /* expired */);
  AddToCache(device_data_3.label(), ParsedDialDeviceDescription(),
             false /* expired */);

  device_description_service_.CleanUpCacheEntries();
  EXPECT_EQ(size_t(1), description_cache_.size());
  EXPECT_TRUE(base::ContainsKey(description_cache_, device_data_3.label()));

  AddToCache(device_data_3.label(), ParsedDialDeviceDescription(),
             true /* expired*/);
  device_description_service_.CleanUpCacheEntries();
  EXPECT_TRUE(description_cache_.empty());
}

TEST_F(DeviceDescriptionServiceTest, TestOnParsedDeviceDescription) {
  GURL app_url("http://192.168.1.1/apps");
  DialDeviceData device_data = CreateDialDeviceData(1);

  // null_ptr
  std::string error_message = "Failed to parse device description XML";
  TestOnParsedDeviceDescription(nullptr, error_message);

  // Empty field
  error_message = "Failed to process fetch result";
  TestOnParsedDeviceDescription(chrome::mojom::DialDeviceDescription::New(),
                                error_message);

  // Valid device description ptr and put in cache
  auto description_ptr = CreateDialDeviceDescriptionPtr(1);
  TestOnParsedDeviceDescription(std::move(description_ptr), "");
  EXPECT_EQ(size_t(1), description_cache_.size());

  // Valid device description ptr and skip cache.
  size_t cache_num = 256;
  for (size_t i = 0; i < cache_num; i++) {
    AddToCache(std::to_string(i), ParsedDialDeviceDescription(),
               false /* expired */);
  }

  EXPECT_EQ(size_t(cache_num + 1), description_cache_.size());
  description_ptr = CreateDialDeviceDescriptionPtr(1);
  TestOnParsedDeviceDescription(std::move(description_ptr), "");
  EXPECT_EQ(size_t(cache_num + 1), description_cache_.size());
}

TEST_F(DeviceDescriptionServiceTest, TestSafeParserProperlyCreated) {
  DialDeviceData device_data_1 = CreateDialDeviceData(1);
  DialDeviceData device_data_2 = CreateDialDeviceData(2);
  DialDeviceData device_data_3 = CreateDialDeviceData(3);

  std::vector<DialDeviceData> devices = {device_data_1, device_data_2,
                                         device_data_3};

  // insert fetchers
  device_description_service()->GetDeviceDescriptions(
      devices, profile_.GetRequestContext());
  auto test_parser = base::MakeUnique<TestSafeDialDeviceDescriptionParser>();
  EXPECT_CALL(*test_parser, Start(_, _)).Times(3);

  EXPECT_FALSE(device_description_service()->parser_);
  SetTestParser(std::move(test_parser));
  OnDeviceDescriptionFetchComplete(1);

  EXPECT_TRUE(device_description_service()->parser_);
  OnDeviceDescriptionFetchComplete(2);
  OnDeviceDescriptionFetchComplete(3);

  EXPECT_FALSE(device_description_service()->parser_);
}

}  // namespace media_router
