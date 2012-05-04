// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/geolocation/fake_access_token_store.h"
#include "content/browser/geolocation/network_location_provider.h"
#include "content/test/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::FakeAccessTokenStore;
using content::Geoposition;

namespace {

// Constants used in multiple tests.
const char kTestServerUrl[] = "https://www.geolocation.test/service";

#if defined(GOOGLE_CHROME_BUILD)
  const char kTestJson[] = "?browser=googlechrome&sensor=true";
  const char kTestBrowser[] = "browser=googlechrome";
#else
  const char kTestJson[] = "?browser=chromium&sensor=true";
  const char kTestBrowser[] = "browser=chromium";
#endif

const char kTestSensor[] = "sensor=true";
// Using #define so we can easily paste this into various other strings.
#define REFERENCE_ACCESS_TOKEN "2:k7j3G6LaL6u_lafw:4iXOeOpTh1glSXe"

// Stops the specified (nested) message loop when the listener is called back.
class MessageLoopQuitListener
    : public LocationProviderBase::ListenerInterface {
 public:
  MessageLoopQuitListener()
      : client_message_loop_(MessageLoop::current()),
        updated_provider_(NULL),
        movement_provider_(NULL) {
    CHECK(client_message_loop_);
  }
  // ListenerInterface
  virtual void LocationUpdateAvailable(LocationProviderBase* provider) {
    EXPECT_EQ(client_message_loop_, MessageLoop::current());
    updated_provider_ = provider;
    client_message_loop_->Quit();
  }
  MessageLoop* client_message_loop_;
  LocationProviderBase* updated_provider_;
  LocationProviderBase* movement_provider_;
};

// A mock implementation of DeviceDataProviderImplBase for testing. Adapted from
// http://gears.googlecode.com/svn/trunk/gears/geolocation/geolocation_test.cc
template<typename DataType>
class MockDeviceDataProviderImpl
    : public DeviceDataProviderImplBase<DataType> {
 public:
  // Factory method for use with DeviceDataProvider::SetFactory.
  static DeviceDataProviderImplBase<DataType>* GetInstance() {
    CHECK(instance_);
    return instance_;
  }

  static MockDeviceDataProviderImpl<DataType>* CreateInstance() {
    CHECK(!instance_);
    instance_ = new MockDeviceDataProviderImpl<DataType>;
    return instance_;
  }

  MockDeviceDataProviderImpl()
      : start_calls_(0),
        stop_calls_(0),
        got_data_(true) {
  }

  virtual ~MockDeviceDataProviderImpl() {
    CHECK(this == instance_);
    instance_ = NULL;
  }

  // DeviceDataProviderImplBase implementation.
  virtual bool StartDataProvider() {
    ++start_calls_;
    return true;
  }
  virtual void StopDataProvider() {
    ++stop_calls_;
  }
  virtual bool GetData(DataType* data_out) {
    CHECK(data_out);
    *data_out = data_;
    return got_data_;
  }

  void SetData(const DataType& new_data) {
    got_data_ = true;
    const bool differs = data_.DiffersSignificantly(new_data);
    data_ = new_data;
    if (differs)
      this->NotifyListeners();
  }

  void set_got_data(bool got_data) { got_data_ = got_data; }
  int start_calls_;
  int stop_calls_;

 private:
  static MockDeviceDataProviderImpl<DataType>* instance_;

  DataType data_;
  bool got_data_;

  DISALLOW_COPY_AND_ASSIGN(MockDeviceDataProviderImpl);
};

template<typename DataType>
MockDeviceDataProviderImpl<DataType>*
MockDeviceDataProviderImpl<DataType>::instance_ = NULL;

// Main test fixture
class GeolocationNetworkProviderTest : public testing::Test {
 public:
  virtual void SetUp() {
    access_token_store_ = new FakeAccessTokenStore;
    radio_data_provider_ =
        MockDeviceDataProviderImpl<RadioData>::CreateInstance();
    wifi_data_provider_ =
        MockDeviceDataProviderImpl<WifiData>::CreateInstance();
  }

  virtual void TearDown() {
    WifiDataProvider::ResetFactory();
    RadioDataProvider::ResetFactory();
  }

  LocationProviderBase* CreateProvider(bool set_permission_granted) {
    LocationProviderBase* provider = NewNetworkLocationProvider(
        access_token_store_.get(),
        NULL,  // No URLContextGetter needed, as using test urlfecther factory.
        test_server_url_,
        access_token_store_->access_token_set_[test_server_url_]);
    if (set_permission_granted)
      provider->OnPermissionGranted();
    return provider;
  }

 protected:
  GeolocationNetworkProviderTest() : test_server_url_(kTestServerUrl) {
    // TODO(joth): Really these should be in SetUp, not here, but they take no
    // effect on Mac OS Release builds if done there. I kid not. Figure out why.
    RadioDataProvider::SetFactory(
        MockDeviceDataProviderImpl<RadioData>::GetInstance);
    WifiDataProvider::SetFactory(
        MockDeviceDataProviderImpl<WifiData>::GetInstance);
  }

  // Returns the current url fetcher (if any) and advances the id ready for the
  // next test step.
  TestURLFetcher* get_url_fetcher_and_advance_id() {
    TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(
            NetworkLocationRequest::url_fetcher_id_for_tests);
    if (fetcher)
      ++NetworkLocationRequest::url_fetcher_id_for_tests;
    return fetcher;
  }

  static int IndexToChannel(int index) { return index + 4; }

  // Creates wifi data containing the specified number of access points, with
  // some differentiating charactistics in each.
  static WifiData CreateReferenceWifiScanData(int ap_count) {
    WifiData data;
    for (int i = 0; i < ap_count; ++i) {
      AccessPointData ap;
      ap.mac_address =
          ASCIIToUTF16(base::StringPrintf("%02d-34-56-78-54-32", i));
      ap.radio_signal_strength = ap_count - i;
      ap.channel = IndexToChannel(i);
      ap.signal_to_noise = i + 42;
      ap.ssid = ASCIIToUTF16("Some nice+network|name\\");
      data.access_point_data.insert(ap);
    }
    return data;
  }

  static std::vector<std::string> CreateReferenceWifiScanDataJson(
      int ap_count, int start_index) {
    std::vector<std::string> wifi_data;
    for (int i = 0; i < ap_count; ++i) {
      std::string wifi_part;
      wifi_part += "wifi=";
      wifi_part += "mac%3A" + base::StringPrintf("%02d-34-56-78-54-32", i);
      wifi_part += "%7Css%3A" + base::IntToString(start_index + ap_count - i);
      wifi_part += "%7Cage%3A0";
      wifi_part += "%7Cchan%3A" + base::IntToString(IndexToChannel(i));
      wifi_part += "%7Csnr%3A" + base::IntToString(i + 42);
      wifi_part += "%7Cssid%3ASome%20nice%2Bnetwork%5C%7Cname%5C%5C";
      wifi_data.push_back(wifi_part);
    }
    return wifi_data;
  }

  static Geoposition CreateReferencePosition(int id) {
    Geoposition pos;
    pos.latitude = id;
    pos.longitude = -(id + 1);
    pos.altitude = 2 * id;
    pos.timestamp = base::Time::Now();
    return pos;
  }

  static void CheckRequestIsValid(const std::string& request_url,
                                  int expected_routers,
                                  int expected_wifi_aps,
                                  int wifi_start_index,
                                  const std::string& expected_access_token) {
    std::vector<std::string> url_tokens;
    EXPECT_EQ(size_t(2), Tokenize(request_url, "?", &url_tokens));
    EXPECT_EQ(kTestServerUrl, url_tokens[0]);

    std::vector<std::string> json_tokens;
    size_t expected_info_tokens = expected_access_token.empty() ? 2 : 3;
    EXPECT_EQ(expected_info_tokens + expected_wifi_aps,
        Tokenize(url_tokens[1], "&", &json_tokens));
    EXPECT_EQ(kTestBrowser, json_tokens[0]);
    EXPECT_EQ(kTestSensor, json_tokens[1]);
    if (!expected_access_token.empty())
      EXPECT_EQ("token=" + expected_access_token, json_tokens[2]);

    std::vector<std::string> expected_json_tokens =
        CreateReferenceWifiScanDataJson(expected_wifi_aps, wifi_start_index);
    EXPECT_EQ(size_t(expected_wifi_aps), expected_json_tokens.size());
    for (size_t i = 0; i < expected_json_tokens.size(); ++i ) {
      std::vector<std::string> actual_wifi_tokens;
      std::vector<std::string> expected_wifi_tokens;
      ReplaceSubstringsAfterOffset(&json_tokens[i + expected_info_tokens],
                                   0, "%7C", "|");
      ReplaceSubstringsAfterOffset(&expected_json_tokens[i],
                                   0, "%7C", "|");
      Tokenize(json_tokens[i + expected_info_tokens],
               "|", &actual_wifi_tokens);
      Tokenize(expected_json_tokens[i], "|", &expected_wifi_tokens);

      // MAC address.
      EXPECT_EQ(expected_wifi_tokens[0], actual_wifi_tokens[0]);
      // Signal Strength.
      EXPECT_EQ(expected_wifi_tokens[1], actual_wifi_tokens[1]);
      int age;
      base::StringToInt(actual_wifi_tokens[2].substr(
                            4, actual_wifi_tokens[2].size() - 4),
                        &age);
      // Age.
      EXPECT_LT(age, 20 * 1000); // Should not take longer than 20 seconds.
      // Channel.
      EXPECT_EQ(expected_wifi_tokens[3], actual_wifi_tokens[3]);
      // Signal to noise ratio.
      EXPECT_EQ(expected_wifi_tokens[4], actual_wifi_tokens[4]);
      // SSID.
      EXPECT_EQ(expected_wifi_tokens[5], actual_wifi_tokens[5]);
      EXPECT_EQ(expected_wifi_tokens[6], actual_wifi_tokens[6]);
    }
    EXPECT_TRUE(GURL(request_url).is_valid());
  }

  const GURL test_server_url_;
  MessageLoop main_message_loop_;
  scoped_refptr<FakeAccessTokenStore> access_token_store_;
  TestURLFetcherFactory url_fetcher_factory_;
  scoped_refptr<MockDeviceDataProviderImpl<RadioData> > radio_data_provider_;
  scoped_refptr<MockDeviceDataProviderImpl<WifiData> > wifi_data_provider_;
};


TEST_F(GeolocationNetworkProviderTest, CreateDestroy) {
  // Test fixture members were SetUp correctly.
  EXPECT_EQ(&main_message_loop_, MessageLoop::current());
  scoped_ptr<LocationProviderBase> provider(CreateProvider(true));
  EXPECT_TRUE(NULL != provider.get());
  provider.reset();
  SUCCEED();
}

TEST_F(GeolocationNetworkProviderTest, StartProvider) {
  scoped_ptr<LocationProviderBase> provider(CreateProvider(true));
  EXPECT_TRUE(provider->StartProvider(false));
  TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);

  EXPECT_EQ(test_server_url_.spec() + kTestJson,
            fetcher->GetOriginalURL().spec());

  CheckRequestIsValid(fetcher->GetOriginalURL().spec(), 0, 0, 0, "");
}

TEST_F(GeolocationNetworkProviderTest, StartProviderLongRequest) {
  scoped_ptr<LocationProviderBase> provider(CreateProvider(true));
  EXPECT_TRUE(provider->StartProvider(false));
  const int kFirstScanAps = 20;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  main_message_loop_.RunAllPending();
  TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);
  // The request url should have been shortened to less than 2048 characters
  // in length by not including access points with the lowest signal strength
  // in the request.
  EXPECT_LT(fetcher->GetOriginalURL().spec().size(), size_t(2048));
  CheckRequestIsValid(fetcher->GetOriginalURL().spec(), 0, 16, 4, "");
}

TEST_F(GeolocationNetworkProviderTest, MultipleStartProvider) {
  scoped_ptr<LocationProviderBase> provider_1(CreateProvider(true));
  scoped_ptr<LocationProviderBase> provider_2(CreateProvider(true));
  ASSERT_TRUE(radio_data_provider_);
  ASSERT_TRUE(wifi_data_provider_);
  EXPECT_EQ(0, radio_data_provider_->start_calls_);
  EXPECT_EQ(0, wifi_data_provider_->start_calls_);
  EXPECT_EQ(0, radio_data_provider_->stop_calls_);
  EXPECT_EQ(0, wifi_data_provider_->stop_calls_);

  // Start first provider.
  EXPECT_TRUE(provider_1->StartProvider(false));
  EXPECT_EQ(1, radio_data_provider_->start_calls_);
  EXPECT_EQ(1, wifi_data_provider_->start_calls_);
  // Start second provider.
  EXPECT_TRUE(provider_2->StartProvider(false));
  EXPECT_EQ(1, radio_data_provider_->start_calls_);
  EXPECT_EQ(1, wifi_data_provider_->start_calls_);

  // Stop first provider.
  provider_1->StopProvider();
  EXPECT_EQ(0, radio_data_provider_->stop_calls_);
  EXPECT_EQ(0, wifi_data_provider_->stop_calls_);
  // Stop second provider.
  provider_2->StopProvider();
  EXPECT_EQ(1, radio_data_provider_->stop_calls_);
  EXPECT_EQ(1, wifi_data_provider_->stop_calls_);
}

TEST_F(GeolocationNetworkProviderTest, MultiRegistrations) {
  // TODO(joth): Strictly belongs in a base-class unit test file.
  MessageLoopQuitListener listener;
  scoped_ptr<LocationProviderBase> provider(CreateProvider(true));
  EXPECT_FALSE(provider->has_listeners());
  provider->RegisterListener(&listener);
  EXPECT_TRUE(provider->has_listeners());
  provider->RegisterListener(&listener);
  EXPECT_TRUE(provider->has_listeners());

  provider->UnregisterListener(&listener);
  EXPECT_TRUE(provider->has_listeners());
  provider->UnregisterListener(&listener);
  EXPECT_FALSE(provider->has_listeners());
}

TEST_F(GeolocationNetworkProviderTest, MultipleWifiScansComplete) {
  scoped_ptr<LocationProviderBase> provider(CreateProvider(true));
  EXPECT_TRUE(provider->StartProvider(false));

  TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);
  EXPECT_EQ(test_server_url_.spec() + kTestJson,
            fetcher->GetOriginalURL().spec());

  // Complete the network request with bad position fix.
  const char* kNoFixNetworkResponse =
      "{"
      "  \"status\": \"ZERO_RESULTS\""
      "}";
  fetcher->set_url(test_server_url_);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(200);  // OK
  fetcher->SetResponseString(kNoFixNetworkResponse);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  Geoposition position;
  provider->GetPosition(&position);
  EXPECT_FALSE(position.Validate());

  // Now wifi data arrives -- SetData will notify listeners.
  const int kFirstScanAps = 6;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  main_message_loop_.RunAllPending();
  fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);
  // The request should have the wifi data.
  CheckRequestIsValid(
      fetcher->GetOriginalURL().spec(), 0, kFirstScanAps, 0, "");

  // Send a reply with good position fix.
  const char* kReferenceNetworkResponse =
      "{"
      "  \"status\": \"OK\","
      "  \"access_token\": \"" REFERENCE_ACCESS_TOKEN "\","
      "  \"accuracy\": 1200.4,"
      "  \"location\": {"
      "    \"lat\": 51.0,"
      "    \"lng\": -0.1"
      "  }"
      "}";
  fetcher->set_url(test_server_url_);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(200);  // OK
  fetcher->SetResponseString(kReferenceNetworkResponse);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  provider->GetPosition(&position);
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(1200.4, position.accuracy);
  EXPECT_FALSE(position.timestamp.is_null());
  EXPECT_TRUE(position.Validate());

  // Token should be in the store.
  EXPECT_EQ(UTF8ToUTF16(REFERENCE_ACCESS_TOKEN),
            access_token_store_->access_token_set_[test_server_url_]);

  // Wifi updated again, with one less AP. This is 'close enough' to the
  // previous scan, so no new request made.
  const int kSecondScanAps = kFirstScanAps - 1;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kSecondScanAps));
  main_message_loop_.RunAllPending();
  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_FALSE(fetcher);

  provider->GetPosition(&position);
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_TRUE(position.Validate());

  // Now a third scan with more than twice the original amount -> new request.
  const int kThirdScanAps = kFirstScanAps * 2 + 1;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kThirdScanAps));
  main_message_loop_.RunAllPending();
  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_TRUE(fetcher);
  CheckRequestIsValid(fetcher->GetOriginalURL().spec(), 0,
                      kThirdScanAps, 0,
                      REFERENCE_ACCESS_TOKEN);
  // ...reply with a network error.

  fetcher->set_url(test_server_url_);
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::FAILED, -1));
  fetcher->set_response_code(200);  // should be ignored
  fetcher->SetResponseString("");
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Error means we now no longer have a fix.
  provider->GetPosition(&position);
  EXPECT_FALSE(position.Validate());

  // Wifi scan returns to original set: should be serviced from cache.
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  main_message_loop_.RunAllPending();
  EXPECT_FALSE(get_url_fetcher_and_advance_id());  // No new request created.

  provider->GetPosition(&position);
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_TRUE(position.Validate());
}

TEST_F(GeolocationNetworkProviderTest, NoRequestOnStartupUntilWifiData) {
  MessageLoopQuitListener listener;
  wifi_data_provider_->set_got_data(false);
  scoped_ptr<LocationProviderBase> provider(CreateProvider(true));
  EXPECT_TRUE(provider->StartProvider(false));
  provider->RegisterListener(&listener);

  main_message_loop_.RunAllPending();
  EXPECT_FALSE(get_url_fetcher_and_advance_id())
      << "Network request should not be created right away on startup when "
         "wifi data has not yet arrived";

  wifi_data_provider_->SetData(CreateReferenceWifiScanData(1));
  main_message_loop_.RunAllPending();
  EXPECT_TRUE(get_url_fetcher_and_advance_id());
}

TEST_F(GeolocationNetworkProviderTest, NewDataReplacesExistingNetworkRequest) {
  // Send initial request with empty device data
  scoped_ptr<LocationProviderBase> provider(CreateProvider(true));
  EXPECT_TRUE(provider->StartProvider(false));
  TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  EXPECT_TRUE(fetcher);

  // Now wifi data arrives; new request should be sent.
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(4));
  main_message_loop_.RunAllPending();
  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_TRUE(fetcher);
}

TEST_F(GeolocationNetworkProviderTest, NetworkRequestDeferredForPermission) {
  scoped_ptr<LocationProviderBase> provider(CreateProvider(false));
  EXPECT_TRUE(provider->StartProvider(false));
  TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  EXPECT_FALSE(fetcher);
  provider->OnPermissionGranted();

  fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);

  EXPECT_EQ(test_server_url_.spec() + kTestJson,
            fetcher->GetOriginalURL().spec());
}

TEST_F(GeolocationNetworkProviderTest,
       NetworkRequestWithWifiDataDeferredForPermission) {
  access_token_store_->access_token_set_[test_server_url_] =
      UTF8ToUTF16(REFERENCE_ACCESS_TOKEN);
  scoped_ptr<LocationProviderBase> provider(CreateProvider(false));
  EXPECT_TRUE(provider->StartProvider(false));
  TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  EXPECT_FALSE(fetcher);

  static const int kScanCount = 4;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kScanCount));
  main_message_loop_.RunAllPending();

  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_FALSE(fetcher);

  provider->OnPermissionGranted();

  fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);

  CheckRequestIsValid(fetcher->GetOriginalURL().spec(), 0,
                      kScanCount, 0, REFERENCE_ACCESS_TOKEN);
}

TEST_F(GeolocationNetworkProviderTest, NetworkPositionCache) {
  NetworkLocationProvider::PositionCache cache;

  const int kCacheSize = NetworkLocationProvider::PositionCache::kMaximumSize;
  for (int i = 1; i < kCacheSize * 2 + 1; ++i) {
    Geoposition pos = CreateReferencePosition(i);
    bool ret = cache.CachePosition(CreateReferenceWifiScanData(i), pos);
    EXPECT_TRUE(ret)  << i;
    const Geoposition* item =
        cache.FindPosition(CreateReferenceWifiScanData(i));
    ASSERT_TRUE(item) << i;
    EXPECT_EQ(pos.latitude, item->latitude)  << i;
    EXPECT_EQ(pos.longitude, item->longitude)  << i;
    if (i <= kCacheSize) {
      // Nothing should have spilled yet; check oldest item is still there.
      EXPECT_TRUE(cache.FindPosition(CreateReferenceWifiScanData(1)));
    } else {
      const int evicted = i - kCacheSize;
      EXPECT_FALSE(cache.FindPosition(CreateReferenceWifiScanData(evicted)));
      EXPECT_TRUE(cache.FindPosition(CreateReferenceWifiScanData(evicted + 1)));
    }
  }
}

}  // namespace
