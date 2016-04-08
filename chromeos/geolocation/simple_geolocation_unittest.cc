// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/geolocation/simple_geolocation_provider.h"
#include "chromeos/geolocation/simple_geolocation_request_test_monitor.h"
#include "chromeos/network/geolocation_handler.h"
#include "chromeos/network/network_handler.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_impl.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

const int kRequestRetryIntervalMilliSeconds = 200;

// This should be different from default to prevent SimpleGeolocationRequest
// from modifying it.
const char kTestGeolocationProviderUrl[] =
    "https://localhost/geolocation/v1/geolocate?";

const char kSimpleResponseBody[] =
    "{\n"
    "  \"location\": {\n"
    "    \"lat\": 51.0,\n"
    "    \"lng\": -0.1\n"
    "  },\n"
    "  \"accuracy\": 1200.4\n"
    "}";
const char kIPOnlyRequestBody[] = "{\"considerIp\": \"true\"}";
const char kOneWiFiAPRequestBody[] =
    "{"
      "\"considerIp\":true,"
      "\"wifiAccessPoints\":["
        "{"
          "\"channel\":1,"
          "\"macAddress\":\"01:00:00:00:00:00\","
          "\"signalStrength\":10,"
          "\"signalToNoiseRatio\":0"
        "}"
      "]"
    "}";
const char kExpectedPosition[] =
    "latitude=51.000000, longitude=-0.100000, accuracy=1200.400000, "
    "error_code=0, error_message='', status=1 (OK)";

const char kWiFiAP1MacAddress[] = "01:00:00:00:00:00";
}  // anonymous namespace

namespace chromeos {

// This is helper class for net::FakeURLFetcherFactory.
class TestGeolocationAPIURLFetcherCallback {
 public:
  TestGeolocationAPIURLFetcherCallback(const GURL& url,
                                       const size_t require_retries,
                                       const std::string& response,
                                       SimpleGeolocationProvider* provider)
      : url_(url),
        require_retries_(require_retries),
        response_(response),
        factory_(nullptr),
        attempts_(0),
        provider_(provider) {}

  std::unique_ptr<net::FakeURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcherDelegate* delegate,
      const std::string& response_data,
      net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    EXPECT_EQ(provider_->requests_.size(), 1U);

    SimpleGeolocationRequest* geolocation_request = provider_->requests_[0];

    const base::TimeDelta base_retry_interval =
        base::TimeDelta::FromMilliseconds(kRequestRetryIntervalMilliSeconds);
    geolocation_request->set_retry_sleep_on_server_error_for_testing(
        base_retry_interval);
    geolocation_request->set_retry_sleep_on_bad_response_for_testing(
        base_retry_interval);

    ++attempts_;
    if (attempts_ > require_retries_) {
      response_code = net::HTTP_OK;
      status = net::URLRequestStatus::SUCCESS;
      factory_->SetFakeResponse(url, response_, response_code, status);
    }
    std::unique_ptr<net::FakeURLFetcher> fetcher(new net::FakeURLFetcher(
        url, delegate, response_, response_code, status));
    scoped_refptr<net::HttpResponseHeaders> download_headers =
        new net::HttpResponseHeaders(std::string());
    download_headers->AddHeader("Content-Type: application/json");
    fetcher->set_response_headers(download_headers);
    return fetcher;
  }

  void Initialize(net::FakeURLFetcherFactory* factory) {
    factory_ = factory;
    factory_->SetFakeResponse(url_,
                              std::string(),
                              net::HTTP_INTERNAL_SERVER_ERROR,
                              net::URLRequestStatus::FAILED);
  }

  size_t attempts() const { return attempts_; }

 private:
  const GURL url_;
  // Respond with OK on required retry attempt.
  const size_t require_retries_;
  std::string response_;
  net::FakeURLFetcherFactory* factory_;
  size_t attempts_;
  SimpleGeolocationProvider* provider_;

  DISALLOW_COPY_AND_ASSIGN(TestGeolocationAPIURLFetcherCallback);
};

// This implements fake Google MAPS Geolocation API remote endpoint.
// Response data is served to SimpleGeolocationProvider via
// net::FakeURLFetcher.
class GeolocationAPIFetcherFactory {
 public:
  GeolocationAPIFetcherFactory(const GURL& url,
                               const std::string& response,
                               const size_t require_retries,
                               SimpleGeolocationProvider* provider) {
    url_callback_.reset(new TestGeolocationAPIURLFetcherCallback(
        url, require_retries, response, provider));
    net::URLFetcherImpl::set_factory(nullptr);
    fetcher_factory_.reset(new net::FakeURLFetcherFactory(
        nullptr,
        base::Bind(&TestGeolocationAPIURLFetcherCallback::CreateURLFetcher,
                   base::Unretained(url_callback_.get()))));
    url_callback_->Initialize(fetcher_factory_.get());
  }

  size_t attempts() const { return url_callback_->attempts(); }

 private:
  std::unique_ptr<TestGeolocationAPIURLFetcherCallback> url_callback_;
  std::unique_ptr<net::FakeURLFetcherFactory> fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationAPIFetcherFactory);
};

class GeolocationReceiver {
 public:
  GeolocationReceiver() : server_error_(false) {}

  void OnRequestDone(const Geoposition& position,
                     bool server_error,
                     const base::TimeDelta elapsed) {
    position_ = position;
    server_error_ = server_error;
    elapsed_ = elapsed;

    message_loop_runner_->Quit();
  }

  void WaitUntilRequestDone() {
    message_loop_runner_.reset(new base::RunLoop);
    message_loop_runner_->Run();
  }

  const Geoposition& position() const { return position_; }
  bool server_error() const { return server_error_; }
  base::TimeDelta elapsed() const { return elapsed_; }

 private:
  Geoposition position_;
  bool server_error_;
  base::TimeDelta elapsed_;
  std::unique_ptr<base::RunLoop> message_loop_runner_;
};

class WiFiTestMonitor : public SimpleGeolocationRequestTestMonitor {
 public:
  WiFiTestMonitor() {}

  void OnRequestCreated(SimpleGeolocationRequest* request) override {}
  void OnStart(SimpleGeolocationRequest* request) override {
    last_request_body_ = request->FormatRequestBodyForTesting();
  }

  const std::string& last_request_body() const { return last_request_body_; }

 private:
  std::string last_request_body_;

  DISALLOW_COPY_AND_ASSIGN(WiFiTestMonitor);
};

class SimpleGeolocationTest : public testing::Test {
 private:
  base::MessageLoop message_loop_;
};

TEST_F(SimpleGeolocationTest, ResponseOK) {
  SimpleGeolocationProvider provider(nullptr,
                                     GURL(kTestGeolocationProviderUrl));

  GeolocationAPIFetcherFactory url_factory(GURL(kTestGeolocationProviderUrl),
                                           std::string(kSimpleResponseBody),
                                           0 /* require_retries */,
                                           &provider);

  GeolocationReceiver receiver;
  provider.RequestGeolocation(base::TimeDelta::FromSeconds(1), false,
                              base::Bind(&GeolocationReceiver::OnRequestDone,
                                         base::Unretained(&receiver)));
  receiver.WaitUntilRequestDone();

  EXPECT_EQ(kExpectedPosition, receiver.position().ToString());
  EXPECT_FALSE(receiver.server_error());
  EXPECT_EQ(1U, url_factory.attempts());
}

TEST_F(SimpleGeolocationTest, ResponseOKWithRetries) {
  SimpleGeolocationProvider provider(nullptr,
                                     GURL(kTestGeolocationProviderUrl));

  GeolocationAPIFetcherFactory url_factory(GURL(kTestGeolocationProviderUrl),
                                           std::string(kSimpleResponseBody),
                                           3 /* require_retries */,
                                           &provider);

  GeolocationReceiver receiver;
  provider.RequestGeolocation(base::TimeDelta::FromSeconds(1), false,
                              base::Bind(&GeolocationReceiver::OnRequestDone,
                                         base::Unretained(&receiver)));
  receiver.WaitUntilRequestDone();
  EXPECT_EQ(kExpectedPosition, receiver.position().ToString());
  EXPECT_FALSE(receiver.server_error());
  EXPECT_EQ(4U, url_factory.attempts());
}

TEST_F(SimpleGeolocationTest, InvalidResponse) {
  SimpleGeolocationProvider provider(nullptr,
                                     GURL(kTestGeolocationProviderUrl));

  GeolocationAPIFetcherFactory url_factory(GURL(kTestGeolocationProviderUrl),
                                           "invalid JSON string",
                                           0 /* require_retries */,
                                           &provider);

  GeolocationReceiver receiver;

  const int timeout_seconds = 1;
  size_t expected_retries = static_cast<size_t>(
      timeout_seconds * 1000 / kRequestRetryIntervalMilliSeconds);
  ASSERT_GE(expected_retries, 2U);

  provider.RequestGeolocation(base::TimeDelta::FromSeconds(timeout_seconds),
                              false,
                              base::Bind(&GeolocationReceiver::OnRequestDone,
                                         base::Unretained(&receiver)));
  receiver.WaitUntilRequestDone();

  EXPECT_EQ(
      "latitude=200.000000, longitude=200.000000, accuracy=-1.000000, "
      "error_code=0, error_message='SimpleGeolocation provider at "
      "'https://localhost/' : JSONReader failed: Line: 1, column: 1, "
      "Unexpected token..', status=4 (TIMEOUT)",
      receiver.position().ToString());
  EXPECT_TRUE(receiver.server_error());
  EXPECT_GE(url_factory.attempts(), 2U);
  if (url_factory.attempts() > expected_retries + 1) {
    LOG(WARNING)
        << "SimpleGeolocationTest::InvalidResponse: Too many attempts ("
        << url_factory.attempts() << "), no more then " << expected_retries + 1
        << " expected.";
  }
  if (url_factory.attempts() < expected_retries - 1) {
    LOG(WARNING)
        << "SimpleGeolocationTest::InvalidResponse: Too little attempts ("
        << url_factory.attempts() << "), greater then " << expected_retries - 1
        << " expected.";
  }
}

TEST_F(SimpleGeolocationTest, NoWiFi) {
  // This initializes DBusThreadManager and markes it "for tests only".
  DBusThreadManager::GetSetterForTesting();
  NetworkHandler::Initialize();

  WiFiTestMonitor requests_monitor;
  SimpleGeolocationRequest::SetTestMonitor(&requests_monitor);

  SimpleGeolocationProvider provider(nullptr,
                                     GURL(kTestGeolocationProviderUrl));

  GeolocationAPIFetcherFactory url_factory(GURL(kTestGeolocationProviderUrl),
                                           std::string(kSimpleResponseBody),
                                           0 /* require_retries */, &provider);

  GeolocationReceiver receiver;
  provider.RequestGeolocation(base::TimeDelta::FromSeconds(1), true,
                              base::Bind(&GeolocationReceiver::OnRequestDone,
                                         base::Unretained(&receiver)));
  receiver.WaitUntilRequestDone();
  EXPECT_EQ(kIPOnlyRequestBody, requests_monitor.last_request_body());

  EXPECT_EQ(kExpectedPosition, receiver.position().ToString());
  EXPECT_FALSE(receiver.server_error());
  EXPECT_EQ(1U, url_factory.attempts());

  NetworkHandler::Shutdown();
  DBusThreadManager::Shutdown();
}

// Test sending of WiFi Access points.
// (This is mostly derived from GeolocationHandlerTest.)
class SimpleGeolocationWiFiTest : public ::testing::TestWithParam<bool> {
 public:
  SimpleGeolocationWiFiTest() : manager_test_(nullptr) {}

  ~SimpleGeolocationWiFiTest() override {}

  void SetUp() override {
    // This initializes DBusThreadManager and markes it "for tests only".
    DBusThreadManager::GetSetterForTesting();
    // Get the test interface for manager / device.
    manager_test_ =
        DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface();
    ASSERT_TRUE(manager_test_);
    geolocation_handler_.reset(new GeolocationHandler());
    geolocation_handler_->Init();
    message_loop_.RunUntilIdle();
  }

  void TearDown() override {
    geolocation_handler_.reset();
    DBusThreadManager::Shutdown();
  }

  bool GetWifiAccessPoints() {
    return geolocation_handler_->GetWifiAccessPoints(&wifi_access_points_,
                                                     nullptr);
  }

  void AddAccessPoint(int idx) {
    base::DictionaryValue properties;
    std::string mac_address =
        base::StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X", idx, 0, 0, 0, 0, 0);
    std::string channel = base::IntToString(idx);
    std::string strength = base::IntToString(idx * 10);
    properties.SetStringWithoutPathExpansion(shill::kGeoMacAddressProperty,
                                             mac_address);
    properties.SetStringWithoutPathExpansion(shill::kGeoChannelProperty,
                                             channel);
    properties.SetStringWithoutPathExpansion(shill::kGeoSignalStrengthProperty,
                                             strength);
    manager_test_->AddGeoNetwork(shill::kTypeWifi, properties);
    message_loop_.RunUntilIdle();
  }

 protected:
  base::MessageLoopForUI message_loop_;
  std::unique_ptr<GeolocationHandler> geolocation_handler_;
  ShillManagerClient::TestInterface* manager_test_;
  WifiAccessPointVector wifi_access_points_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleGeolocationWiFiTest);
};

// Parameter is enable/disable sending of WiFi data.
TEST_P(SimpleGeolocationWiFiTest, WiFiExists) {
  NetworkHandler::Initialize();

  WiFiTestMonitor requests_monitor;
  SimpleGeolocationRequest::SetTestMonitor(&requests_monitor);

  SimpleGeolocationProvider provider(nullptr,
                                     GURL(kTestGeolocationProviderUrl));

  GeolocationAPIFetcherFactory url_factory(GURL(kTestGeolocationProviderUrl),
                                           std::string(kSimpleResponseBody),
                                           0 /* require_retries */, &provider);
  {
    GeolocationReceiver receiver;
    provider.RequestGeolocation(base::TimeDelta::FromSeconds(1), GetParam(),
                                base::Bind(&GeolocationReceiver::OnRequestDone,
                                           base::Unretained(&receiver)));
    receiver.WaitUntilRequestDone();
    EXPECT_EQ(kIPOnlyRequestBody, requests_monitor.last_request_body());

    EXPECT_EQ(kExpectedPosition, receiver.position().ToString());
    EXPECT_FALSE(receiver.server_error());
    EXPECT_EQ(1U, url_factory.attempts());
  }

  // Add an acces point.
  AddAccessPoint(1);
  message_loop_.RunUntilIdle();
  // Inititial call should return false and request access points.
  EXPECT_FALSE(GetWifiAccessPoints());
  message_loop_.RunUntilIdle();
  // Second call should return true since we have an access point.
  EXPECT_TRUE(GetWifiAccessPoints());
  ASSERT_EQ(1u, wifi_access_points_.size());
  EXPECT_EQ(kWiFiAP1MacAddress, wifi_access_points_[0].mac_address);
  EXPECT_EQ(1, wifi_access_points_[0].channel);

  {
    GeolocationReceiver receiver;
    provider.RequestGeolocation(base::TimeDelta::FromSeconds(1), GetParam(),
                                base::Bind(&GeolocationReceiver::OnRequestDone,
                                           base::Unretained(&receiver)));
    receiver.WaitUntilRequestDone();
    if (GetParam()) {
      // Sending WiFi data is enabled.
      EXPECT_EQ(kOneWiFiAPRequestBody, requests_monitor.last_request_body());
    } else {
      // Sending WiFi data is disabled.
      EXPECT_EQ(kIPOnlyRequestBody, requests_monitor.last_request_body());
    }

    EXPECT_EQ(kExpectedPosition, receiver.position().ToString());
    EXPECT_FALSE(receiver.server_error());
    // This is total.
    EXPECT_EQ(2U, url_factory.attempts());
  }
  NetworkHandler::Shutdown();
}

// This test verifies that WiFi data is sent only if sending was requested.
INSTANTIATE_TEST_CASE_P(EnableDisableSendingWifiData,
                        SimpleGeolocationWiFiTest,
                        testing::Bool());

}  // namespace chromeos
