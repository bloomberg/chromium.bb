// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "content/browser/geolocation/fake_access_token_store.h"
#include "content/browser/geolocation/network_location_provider.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Constants used in multiple tests.
const char kTestServerUrl[] = "https://www.geolocation.test/service";
const char kTestHost[] = "myclienthost.test";
const char kTestHostUrl[] = "http://myclienthost.test/some/path";
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
    URLFetcher::set_factory(&url_fetcher_factory_);
    access_token_store_ = new FakeAccessTokenStore;
    gateway_data_provider_ =
        MockDeviceDataProviderImpl<GatewayData>::CreateInstance();
    radio_data_provider_ =
        MockDeviceDataProviderImpl<RadioData>::CreateInstance();
    wifi_data_provider_ =
        MockDeviceDataProviderImpl<WifiData>::CreateInstance();
  }

  virtual void TearDown() {
    WifiDataProvider::ResetFactory();
    RadioDataProvider::ResetFactory();
    GatewayDataProvider::ResetFactory();
    URLFetcher::set_factory(NULL);
  }

  LocationProviderBase* CreateProvider(bool set_permission_granted) {
    LocationProviderBase* provider = NewNetworkLocationProvider(
        access_token_store_.get(),
        NULL,  // No URLContextGetter needed, as using test urlfecther factory.
        test_server_url_,
        access_token_store_->access_token_set_[test_server_url_]);
    if (set_permission_granted)
      provider->OnPermissionGranted(GURL(kTestHostUrl));
    return provider;
  }

 protected:
  GeolocationNetworkProviderTest() : test_server_url_(kTestServerUrl) {
    // TODO(joth): Really these should be in SetUp, not here, but they take no
    // effect on Mac OS Release builds if done there. I kid not. Figure out why.
    GatewayDataProvider::SetFactory(
        MockDeviceDataProviderImpl<GatewayData>::GetInstance);
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

  static int IndexToChannal(int index) { return index + 4; }

  // Creates wifi data containing the specified number of access points, with
  // some differentiating charactistics in each.
  static WifiData CreateReferenceWifiScanData(int ap_count) {
    WifiData data;
    for (int i = 0; i < ap_count; ++i) {
      AccessPointData ap;
      ap.mac_address = ASCIIToUTF16(StringPrintf("%02d-34-56-78-54-32", i));
      ap.radio_signal_strength = i;
      ap.channel = IndexToChannal(i);
      ap.signal_to_noise = i + 42;
      ap.ssid = ASCIIToUTF16("Some nice network");
      data.access_point_data.insert(ap);
    }
    return data;
  }

  // Creates gateway data containing the specified number of routers, with
  // some differentiating charactistics in each.
  static GatewayData CreateReferenceRouterData(int router_count) {
    GatewayData data;
    for (int i = 0; i < router_count; ++i) {
      RouterData router;
      router.mac_address =
          ASCIIToUTF16(StringPrintf("%02d-34-56-78-54-32", i));
      data.router_data.insert(router);
    }
    return data;
  }

  static Geoposition CreateReferencePosition(int id) {
    Geoposition pos;
    pos.latitude = id;
    pos.longitude = -(id + 1);
    pos.altitude = 2 * id;
    pos.timestamp = base::Time::Now();
    return pos;
  }

  static void ParseGatewayRequest(const std::string& request_data,
                                  GatewayData* gateway_data_out) {
    scoped_ptr<Value> value(base::JSONReader::Read(request_data, false));
    EXPECT_TRUE(value != NULL);
    EXPECT_EQ(Value::TYPE_DICTIONARY, value->GetType());
    DictionaryValue* dictionary = static_cast<DictionaryValue*>(value.get());
    std::string attr_value;
    EXPECT_TRUE(dictionary->GetString("version", &attr_value));
    EXPECT_EQ(attr_value, "1.1.0");
    EXPECT_TRUE(dictionary->GetString("host", &attr_value));
    EXPECT_EQ(attr_value, kTestHost);
    // Everything else is optional.
    ListValue* gateways;
    if (dictionary->GetList("gateways", &gateways)) {
      int i = 0;
      for (ListValue::const_iterator it = gateways->begin();
           it < gateways->end(); ++it, ++i) {
        EXPECT_EQ(Value::TYPE_DICTIONARY, (*it)->GetType());
        DictionaryValue* gateway = static_cast<DictionaryValue*>(*it);
        RouterData data;
        gateway->GetString("mac_address", &data.mac_address);
        gateway_data_out->router_data.insert(data);
      }
    } else {
      gateway_data_out->router_data.clear();
    }
  }

  static void ParseWifiRequest(const std::string& request_data,
                               WifiData* wifi_data_out,
                               int* max_age_out,
                               std::string* access_token_out) {
    CHECK(wifi_data_out && max_age_out && access_token_out);
    scoped_ptr<Value> value(base::JSONReader::Read(request_data, false));
    EXPECT_TRUE(value != NULL);
    EXPECT_EQ(Value::TYPE_DICTIONARY, value->GetType());
    DictionaryValue* dictionary = static_cast<DictionaryValue*>(value.get());
    std::string attr_value;
    EXPECT_TRUE(dictionary->GetString("version", &attr_value));
    EXPECT_EQ(attr_value, "1.1.0");
    EXPECT_TRUE(dictionary->GetString("host", &attr_value));
    EXPECT_EQ(attr_value, kTestHost);
    // Everything else is optional.
    ListValue* wifi_aps;
    *max_age_out = kint32min;
    if (dictionary->GetList("wifi_towers", &wifi_aps)) {
      int i = 0;
      for (ListValue::const_iterator it = wifi_aps->begin();
           it < wifi_aps->end(); ++it, ++i) {
        EXPECT_EQ(Value::TYPE_DICTIONARY, (*it)->GetType());
        DictionaryValue* ap = static_cast<DictionaryValue*>(*it);
        AccessPointData data;
        ap->GetString("mac_address", &data.mac_address);
        ap->GetInteger("signal_strength", &data.radio_signal_strength);
        int age = kint32min;
        ap->GetInteger("age", &age);
        if (age > *max_age_out)
          *max_age_out = age;
        ap->GetInteger("channel", &data.channel);
        ap->GetInteger("signal_to_noise", &data.signal_to_noise);
        ap->GetString("ssid", &data.ssid);
        wifi_data_out->access_point_data.insert(data);
      }
    } else {
      wifi_data_out->access_point_data.clear();
    }
    if (!dictionary->GetString("access_token", access_token_out))
      access_token_out->clear();
  }

  static void CheckEmptyRequestIsValid(const std::string& request_data) {
    WifiData wifi_aps;
    std::string access_token;
    int max_age;
    ParseWifiRequest(request_data, &wifi_aps, &max_age, &access_token);
    EXPECT_EQ(kint32min, max_age);
    EXPECT_EQ(0, static_cast<int>(wifi_aps.access_point_data.size()));
    EXPECT_TRUE(access_token.empty());
  }

  static void CheckRequestIsValid(const std::string& request_data,
                                  int expected_routers,
                                  int expected_wifi_aps,
                                  const std::string& expected_access_token) {
    WifiData wifi_aps;
    std::string access_token;
    int max_age;
    ParseWifiRequest(request_data, &wifi_aps, &max_age, &access_token);
    EXPECT_EQ(expected_wifi_aps,
              static_cast<int>(wifi_aps.access_point_data.size()));
    if (expected_wifi_aps > 0) {
      EXPECT_GE(max_age, 0) << "Age must not be negative.";
      EXPECT_LT(max_age, 10 * 1000) << "This test really shouldn't take 10s.";
      WifiData expected_data = CreateReferenceWifiScanData(expected_wifi_aps);
      WifiData::AccessPointDataSet::const_iterator expected =
          expected_data.access_point_data.begin();
      WifiData::AccessPointDataSet::const_iterator actual =
          wifi_aps.access_point_data.begin();
      for (int i = 0; i < expected_wifi_aps; ++i) {
        EXPECT_EQ(expected->mac_address, actual->mac_address) << i;
        EXPECT_EQ(expected->radio_signal_strength,
            actual->radio_signal_strength) << i;
        EXPECT_EQ(expected->channel, actual->channel) << i;
        EXPECT_EQ(expected->signal_to_noise, actual->signal_to_noise) << i;
        EXPECT_EQ(expected->ssid, actual->ssid) << i;
        ++expected;
        ++actual;
      }
    } else {
      EXPECT_EQ(max_age, kint32min);
    }
    EXPECT_EQ(expected_access_token, access_token);

    GatewayData gateway_data;
    ParseGatewayRequest(request_data, &gateway_data);
    EXPECT_EQ(expected_routers,
              static_cast<int>(gateway_data.router_data.size()));
    GatewayData expected_data = CreateReferenceRouterData(expected_routers);
    GatewayData::RouterDataSet::const_iterator expected =
        expected_data.router_data.begin();
    GatewayData::RouterDataSet::const_iterator actual =
        gateway_data.router_data.begin();
    for (int i = 0; i < expected_routers; ++i) {
      EXPECT_EQ(expected->mac_address, actual->mac_address) << i;
      ++expected;
      ++actual;
    }
  }

  const GURL test_server_url_;
  MessageLoop main_message_loop_;
  scoped_refptr<FakeAccessTokenStore> access_token_store_;
  TestURLFetcherFactory url_fetcher_factory_;
  scoped_refptr<MockDeviceDataProviderImpl<GatewayData> >
      gateway_data_provider_;
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

  EXPECT_EQ(test_server_url_, fetcher->original_url());

  // No wifi data so expect an empty request.
  CheckEmptyRequestIsValid(fetcher->upload_data());
}

TEST_F(GeolocationNetworkProviderTest, MultipleStartProvider) {
  scoped_ptr<LocationProviderBase> provider_1(CreateProvider(true));
  scoped_ptr<LocationProviderBase> provider_2(CreateProvider(true));
  ASSERT_TRUE(gateway_data_provider_);
  ASSERT_TRUE(radio_data_provider_);
  ASSERT_TRUE(wifi_data_provider_);
  EXPECT_EQ(0, gateway_data_provider_->start_calls_);
  EXPECT_EQ(0, radio_data_provider_->start_calls_);
  EXPECT_EQ(0, wifi_data_provider_->start_calls_);
  EXPECT_EQ(0, gateway_data_provider_->stop_calls_);
  EXPECT_EQ(0, radio_data_provider_->stop_calls_);
  EXPECT_EQ(0, wifi_data_provider_->stop_calls_);

  // Start first provider.
  EXPECT_TRUE(provider_1->StartProvider(false));
  EXPECT_EQ(1, gateway_data_provider_->start_calls_);
  EXPECT_EQ(1, radio_data_provider_->start_calls_);
  EXPECT_EQ(1, wifi_data_provider_->start_calls_);
  // Start second provider.
  EXPECT_TRUE(provider_2->StartProvider(false));
  EXPECT_EQ(1, gateway_data_provider_->start_calls_);
  EXPECT_EQ(1, radio_data_provider_->start_calls_);
  EXPECT_EQ(1, wifi_data_provider_->start_calls_);

  // Stop first provider.
  provider_1->StopProvider();
  EXPECT_EQ(0, gateway_data_provider_->stop_calls_);
  EXPECT_EQ(0, radio_data_provider_->stop_calls_);
  EXPECT_EQ(0, wifi_data_provider_->stop_calls_);
  // Stop second provider.
  provider_2->StopProvider();
  EXPECT_EQ(1, gateway_data_provider_->stop_calls_);
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
  CheckEmptyRequestIsValid(fetcher->upload_data());
  // Complete the network request with bad position fix.
  const char* kNoFixNetworkResponse =
      "{"
      "  \"location\": null,"
      "  \"access_token\": \"" REFERENCE_ACCESS_TOKEN "\""
      "}";
  fetcher->delegate()->OnURLFetchComplete(
      fetcher, test_server_url_, net::URLRequestStatus(), 200,  // OK
      ResponseCookies(), kNoFixNetworkResponse);

  // This should have set the access token anyhow
  EXPECT_EQ(UTF8ToUTF16(REFERENCE_ACCESS_TOKEN),
            access_token_store_->access_token_set_[test_server_url_]);

  Geoposition position;
  provider->GetPosition(&position);
  EXPECT_FALSE(position.IsValidFix());

  // Now wifi data arrives -- SetData will notify listeners.
  const int kFirstScanAps = 6;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  main_message_loop_.RunAllPending();
  fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);
  // The request should have access token (set previously) and the wifi data.
  CheckRequestIsValid(fetcher->upload_data(), 0,
                      kFirstScanAps,
                      REFERENCE_ACCESS_TOKEN);

  // Send a reply with good position fix.
  const char* kReferenceNetworkResponse =
      "{"
      "  \"location\": {"
      "    \"latitude\": 51.0,"
      "    \"longitude\": -0.1,"
      "    \"altitude\": 30.1,"
      "    \"accuracy\": 1200.4,"
      "    \"altitude_accuracy\": 10.6"
      "  }"
      "}";
  fetcher->delegate()->OnURLFetchComplete(
      fetcher, test_server_url_, net::URLRequestStatus(), 200,  // OK
      ResponseCookies(), kReferenceNetworkResponse);

  provider->GetPosition(&position);
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(30.1, position.altitude);
  EXPECT_EQ(1200.4, position.accuracy);
  EXPECT_EQ(10.6, position.altitude_accuracy);
  EXPECT_TRUE(position.is_valid_timestamp());
  EXPECT_TRUE(position.IsValidFix());

  // Token should still be in the store.
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
  EXPECT_TRUE(position.IsValidFix());

  // Now a third scan with more than twice the original amount -> new request.
  const int kThirdScanAps = kFirstScanAps * 2 + 1;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kThirdScanAps));
  main_message_loop_.RunAllPending();
  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_TRUE(fetcher);
  // ...reply with a network error.
  fetcher->delegate()->OnURLFetchComplete(
      fetcher, test_server_url_,
      net::URLRequestStatus(net::URLRequestStatus::FAILED, -1),
      200,  // should be ignored
      ResponseCookies(), "");

  // Error means we now no longer have a fix.
  provider->GetPosition(&position);
  EXPECT_FALSE(position.is_valid_latlong());
  EXPECT_FALSE(position.IsValidFix());

  // Wifi scan returns to original set: should be serviced from cache.
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kFirstScanAps));
  main_message_loop_.RunAllPending();
  EXPECT_FALSE(get_url_fetcher_and_advance_id());  // No new request created.

  provider->GetPosition(&position);
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_TRUE(position.IsValidFix());
}

TEST_F(GeolocationNetworkProviderTest, GatewayAndWifiScans) {
  scoped_ptr<LocationProviderBase> provider(CreateProvider(true));
  EXPECT_TRUE(provider->StartProvider(false));

  TestURLFetcher* fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);
  CheckEmptyRequestIsValid(fetcher->upload_data());
  // Complete the network request with bad position fix (using #define so we
  // can paste this into various other strings below)
  #define REFERENCE_ACCESS_TOKEN "2:k7j3G6LaL6u_lafw:4iXOeOpTh1glSXe"
  const char* kNoFixNetworkResponse =
      "{"
      "  \"location\": null,"
      "  \"access_token\": \"" REFERENCE_ACCESS_TOKEN "\""
      "}";
  fetcher->delegate()->OnURLFetchComplete(
      fetcher, test_server_url_, net::URLRequestStatus(), 200,  // OK
      ResponseCookies(), kNoFixNetworkResponse);

  // This should have set the access token anyhow
  EXPECT_EQ(UTF8ToUTF16(REFERENCE_ACCESS_TOKEN),
            access_token_store_->access_token_set_[test_server_url_]);

  Geoposition position;
  provider->GetPosition(&position);
  EXPECT_FALSE(position.IsValidFix());

  // Now gateway data arrives -- SetData will notify listeners.
  const int kFirstScanRouters = 1;
  gateway_data_provider_->SetData(
      CreateReferenceRouterData(kFirstScanRouters));
  main_message_loop_.RunAllPending();
  fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);
  // The request should have access token (set previously) and the
  // gateway data.
  CheckRequestIsValid(fetcher->upload_data(), kFirstScanRouters,
                      0, REFERENCE_ACCESS_TOKEN);

  // Send a reply with good position fix.
  const char* kReferenceNetworkResponse_1 =
      "{"
      "  \"location\": {"
      "    \"latitude\": 51.0,"
      "    \"longitude\": -0.1,"
      "    \"altitude\": 30.1,"
      "    \"accuracy\": 1200.4,"
      "    \"altitude_accuracy\": 10.6"
      "  }"
      "}";
  fetcher->delegate()->OnURLFetchComplete(
      fetcher, test_server_url_, net::URLRequestStatus(), 200,  // OK
      ResponseCookies(), kReferenceNetworkResponse_1);

  provider->GetPosition(&position);
  EXPECT_EQ(51.0, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(30.1, position.altitude);
  EXPECT_EQ(1200.4, position.accuracy);
  EXPECT_EQ(10.6, position.altitude_accuracy);
  EXPECT_TRUE(position.is_valid_timestamp());
  EXPECT_TRUE(position.IsValidFix());

  // Token should still be in the store.
  EXPECT_EQ(UTF8ToUTF16(REFERENCE_ACCESS_TOKEN),
            access_token_store_->access_token_set_[test_server_url_]);

  // Gateway updated again, with one more router. This is a significant change
  // so a new request is made.
  const int kSecondScanRouters = kFirstScanRouters + 1;
  gateway_data_provider_->SetData(
      CreateReferenceRouterData(kSecondScanRouters));
  main_message_loop_.RunAllPending();
  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_TRUE(fetcher);

  CheckRequestIsValid(fetcher->upload_data(), kSecondScanRouters,
                      0, REFERENCE_ACCESS_TOKEN);

  // Send a reply with good position fix.
  const char* kReferenceNetworkResponse_2 =
      "{"
      "  \"location\": {"
      "    \"latitude\": 51.1,"
      "    \"longitude\": -0.1,"
      "    \"altitude\": 30.2,"
      "    \"accuracy\": 1100.4,"
      "    \"altitude_accuracy\": 10.6"
      "  }"
      "}";
  fetcher->delegate()->OnURLFetchComplete(
      fetcher, test_server_url_, net::URLRequestStatus(), 200,  // OK
      ResponseCookies(), kReferenceNetworkResponse_2);

  provider->GetPosition(&position);
  EXPECT_EQ(51.1, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(30.2, position.altitude);
  EXPECT_EQ(1100.4, position.accuracy);
  EXPECT_EQ(10.6, position.altitude_accuracy);
  EXPECT_TRUE(position.is_valid_timestamp());
  EXPECT_TRUE(position.IsValidFix());

  // Now add new wifi scan data.
  const int kScanAps = 4;
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(kScanAps));
  main_message_loop_.RunAllPending();
  fetcher = get_url_fetcher_and_advance_id();
  EXPECT_TRUE(fetcher);
  CheckRequestIsValid(fetcher->upload_data(), kSecondScanRouters,
                      kScanAps, REFERENCE_ACCESS_TOKEN);

  // Send a reply with good position fix.
  const char* kReferenceNetworkResponse_3 =
      "{"
      "  \"location\": {"
      "    \"latitude\": 51.3,"
      "    \"longitude\": -0.1,"
      "    \"altitude\": 30.2,"
      "    \"accuracy\": 50.4,"
      "    \"altitude_accuracy\": 10.6"
      "  }"
      "}";
  fetcher->delegate()->OnURLFetchComplete(
      fetcher, test_server_url_, net::URLRequestStatus(), 200,  // OK
      ResponseCookies(), kReferenceNetworkResponse_3);

  provider->GetPosition(&position);
  EXPECT_EQ(51.3, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(30.2, position.altitude);
  EXPECT_EQ(50.4, position.accuracy);
  EXPECT_EQ(10.6, position.altitude_accuracy);
  EXPECT_TRUE(position.is_valid_timestamp());
  EXPECT_TRUE(position.IsValidFix());

  // Wifi scan returns no access points found: should be serviced from cache.
  wifi_data_provider_->SetData(CreateReferenceWifiScanData(0));
  main_message_loop_.RunAllPending();
  EXPECT_FALSE(get_url_fetcher_and_advance_id());  // No new request created.

  provider->GetPosition(&position);
  EXPECT_EQ(51.1, position.latitude);
  EXPECT_EQ(-0.1, position.longitude);
  EXPECT_EQ(30.2, position.altitude);
  EXPECT_EQ(1100.4, position.accuracy);
  EXPECT_EQ(10.6, position.altitude_accuracy);
  EXPECT_TRUE(position.is_valid_timestamp());
  EXPECT_TRUE(position.IsValidFix());
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
  provider->OnPermissionGranted(GURL(kTestHostUrl));

  fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);

  EXPECT_EQ(test_server_url_, fetcher->original_url());

  // No wifi data so expect an empty request.
  CheckEmptyRequestIsValid(fetcher->upload_data());
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

  provider->OnPermissionGranted(GURL(kTestHostUrl));

  fetcher = get_url_fetcher_and_advance_id();
  ASSERT_TRUE(fetcher != NULL);

  EXPECT_EQ(test_server_url_, fetcher->original_url());

  CheckRequestIsValid(fetcher->upload_data(), 0,
                      kScanCount, REFERENCE_ACCESS_TOKEN);
}

TEST_F(GeolocationNetworkProviderTest, NetworkPositionCache) {
  NetworkLocationProvider::PositionCache cache;

  const int kCacheSize = NetworkLocationProvider::PositionCache::kMaximumSize;
  for (int i = 0; i < kCacheSize * 2 + 1; ++i) {
    Geoposition pos = CreateReferencePosition(i);
    bool ret = cache.CachePosition(CreateReferenceRouterData(2),
                                   CreateReferenceWifiScanData(i), pos);
    EXPECT_TRUE(ret)  << i;
    const Geoposition* item = cache.FindPosition(
        CreateReferenceRouterData(2),
        CreateReferenceWifiScanData(i));
    ASSERT_TRUE(item) << i;
    EXPECT_EQ(pos.latitude, item->latitude)  << i;
    EXPECT_EQ(pos.longitude, item->longitude)  << i;
    if (i < kCacheSize) {
      // Nothing should have spilled yet; check oldest item is still there.
      EXPECT_TRUE(cache.FindPosition(CreateReferenceRouterData(2),
                                     CreateReferenceWifiScanData(0)));
    } else {
      const int evicted = i - kCacheSize;
      EXPECT_FALSE(cache.FindPosition(CreateReferenceRouterData(2),
                                      CreateReferenceWifiScanData(evicted)));
      EXPECT_TRUE(cache.FindPosition(CreateReferenceRouterData(2),
                                     CreateReferenceWifiScanData(evicted + 1)));
    }
  }
}

}  // namespace
