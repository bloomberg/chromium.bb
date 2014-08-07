// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/browser/extensions/api/gcd_private/gcd_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"
#include "chrome/browser/local_discovery/wifi/mock_wifi_manager.h"
#include "chrome/common/extensions/api/mdns.h"
#include "extensions/common/switches.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(ENABLE_MDNS)
#include "chrome/browser/local_discovery/test_service_discovery_client.h"
#endif  // ENABLE_MDNS

namespace api = extensions::api;

using testing::Invoke;

namespace {

const char kCloudPrintResponse[] =
    "{"
    "   \"success\": true,"
    "   \"printers\": ["
    "     {\"id\" : \"someCloudPrintID\","
    "      \"displayName\": \"someCloudPrintDisplayName\","
    "      \"description\": \"someCloudPrintDescription\"}"
    "    ]"
    "}";

const char kGCDResponse[] =
    "{"
    "\"kind\": \"clouddevices#devicesListResponse\","
    "\"devices\": [{"
    "  \"kind\": \"clouddevices#device\","
    "  \"id\": \"someGCDID\","
    "  \"deviceKind\": \"someType\","
    "  \"creationTimeMs\": \"123\","
    "  \"systemName\": \"someGCDDisplayName\","
    "  \"owner\": \"user@domain.com\","
    "  \"description\": \"someGCDDescription\","
    "  \"state\": {"
    "  \"base\": {"
    "    \"connectionStatus\": \"offline\""
    "   }"
    "  },"
    "  \"channel\": {"
    "  \"supportedType\": \"xmpp\""
    "  },"
    "  \"personalizedInfo\": {"
    "   \"maxRole\": \"owner\""
    "  }}]}";

const char kPrivetInfoResponse[] =
    "{"
    "\"x-privet-token\": \"sample\""
    "}";

const char kPrivetPingResponse[] =
    "{"
    "\"response\": \"pong\""
    "}";

#if defined(ENABLE_MDNS)

const uint8 kAnnouncePacket[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x80, 0x00,  // Standard query response, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x05,  // 5 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs
    0x07, '_',  'p',  'r',  'i',  'v',  'e',  't',  0x04, '_',
    't',  'c',  'p',  0x05, 'l',  'o',  'c',  'a',  'l',  0x00,
    0x00, 0x0c,              // TYPE is PTR.
    0x00, 0x01,              // CLASS is IN.
    0x00, 0x00,              // TTL (4 bytes) is 32768 second.
    0x10, 0x00, 0x00, 0x0c,  // RDLENGTH is 12 bytes.
    0x09, 'm',  'y',  'S',  'e',  'r',  'v',  'i',  'c',  'e',
    0xc0, 0x0c, 0x09, 'm',  'y',  'S',  'e',  'r',  'v',  'i',
    'c',  'e',  0xc0, 0x0c, 0x00, 0x10,  // TYPE is TXT.
    0x00, 0x01,                          // CLASS is IN.
    0x00, 0x00,                          // TTL (4 bytes) is 32768 seconds.
    0x01, 0x00, 0x00, 0x41,              // RDLENGTH is 69 bytes.
    0x03, 'i',  'd',  '=',  0x10, 't',  'y',  '=',  'S',  'a',
    'm',  'p',  'l',  'e',  ' ',  'd',  'e',  'v',  'i',  'c',
    'e',  0x1e, 'n',  'o',  't',  'e',  '=',  'S',  'a',  'm',
    'p',  'l',  'e',  ' ',  'd',  'e',  'v',  'i',  'c',  'e',
    ' ',  'd',  'e',  's',  'c',  'r',  'i',  'p',  't',  'i',
    'o',  'n',  0x0c, 't',  'y',  'p',  'e',  '=',  'p',  'r',
    'i',  'n',  't',  'e',  'r',  0x09, 'm',  'y',  'S',  'e',
    'r',  'v',  'i',  'c',  'e',  0xc0, 0x0c, 0x00, 0x21,  // Type is SRV
    0x00, 0x01,                                            // CLASS is IN
    0x00, 0x00,                          // TTL (4 bytes) is 32768 second.
    0x10, 0x00, 0x00, 0x17,              // RDLENGTH is 23
    0x00, 0x00, 0x00, 0x00, 0x22, 0xb8,  // port 8888
    0x09, 'm',  'y',  'S',  'e',  'r',  'v',  'i',  'c',  'e',
    0x05, 'l',  'o',  'c',  'a',  'l',  0x00, 0x09, 'm',  'y',
    'S',  'e',  'r',  'v',  'i',  'c',  'e',  0x05, 'l',  'o',
    'c',  'a',  'l',  0x00, 0x00, 0x01,  // Type is A
    0x00, 0x01,                          // CLASS is IN
    0x00, 0x00,                          // TTL (4 bytes) is 32768 second.
    0x10, 0x00, 0x00, 0x04,              // RDLENGTH is 4
    0x01, 0x02, 0x03, 0x04,              // 1.2.3.4
    0x09, 'm',  'y',  'S',  'e',  'r',  'v',  'i',  'c',  'e',
    0x05, 'l',  'o',  'c',  'a',  'l',  0x00, 0x00, 0x1C,  // Type is AAAA
    0x00, 0x01,                                            // CLASS is IN
    0x00, 0x00,              // TTL (4 bytes) is 32768 second.
    0x10, 0x00, 0x00, 0x10,  // RDLENGTH is 16
    0x01, 0x02, 0x03, 0x04,  // 1.2.3.4
    0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04, 0x01, 0x02,
    0x03, 0x04,
};

const uint8 kGoodbyePacket[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x80, 0x00,  // Standard query response, RA, no error
    0x00, 0x00,  // No questions (for simplicity)
    0x00, 0x02,  // 1 RR (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs
    0x07, '_',  'p',  'r',  'i',  'v',  'e', 't',  0x04, '_',  't',  'c',
    'p',  0x05, 'l',  'o',  'c',  'a',  'l', 0x00, 0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,                                                 // CLASS is IN.
    0x00, 0x00,              // TTL (4 bytes) is 0 seconds.
    0x00, 0x00, 0x00, 0x0c,  // RDLENGTH is 12 bytes.
    0x09, 'm',  'y',  'S',  'e',  'r',  'v', 'i',  'c',  'e',  0xc0, 0x0c,
    0x09, 'm',  'y',  'S',  'e',  'r',  'v', 'i',  'c',  'e',  0xc0, 0x0c,
    0x00, 0x21,                          // Type is SRV
    0x00, 0x01,                          // CLASS is IN
    0x00, 0x00,                          // TTL (4 bytes) is 0 seconds.
    0x00, 0x00, 0x00, 0x17,              // RDLENGTH is 23
    0x00, 0x00, 0x00, 0x00, 0x22, 0xb8,  // port 8888
    0x09, 'm',  'y',  'S',  'e',  'r',  'v', 'i',  'c',  'e',  0x05, 'l',
    'o',  'c',  'a',  'l',  0x00,
};

const uint8 kQueryPacket[] = {
    // Header
    0x00, 0x00,  // ID is zeroed out
    0x00, 0x00,  // No flags.
    0x00, 0x01,  // One question.
    0x00, 0x00,  // 0 RRs (answers)
    0x00, 0x00,  // 0 authority RRs
    0x00, 0x00,  // 0 additional RRs

    // Question
    // This part is echoed back from the respective query.
    0x07, '_',  'p', 'r', 'i', 'v', 'e', 't',  0x04, '_',  't', 'c',
    'p',  0x05, 'l', 'o', 'c', 'a', 'l', 0x00, 0x00, 0x0c,  // TYPE is PTR.
    0x00, 0x01,                                             // CLASS is IN.
};

#endif  // ENABLE_MDNS

// Sentinel value to signify the request should fail.
const char kResponseValueFailure[] = "FAILURE";

class FakeGCDApiFlowFactory
    : public extensions::GcdPrivateAPI::GCDApiFlowFactoryForTests {
 public:
  FakeGCDApiFlowFactory() {
    extensions::GcdPrivateAPI::SetGCDApiFlowFactoryForTests(this);
  }

  virtual ~FakeGCDApiFlowFactory() {
    extensions::GcdPrivateAPI::SetGCDApiFlowFactoryForTests(NULL);
  }

  virtual scoped_ptr<local_discovery::GCDApiFlow> CreateGCDApiFlow() OVERRIDE {
    return scoped_ptr<local_discovery::GCDApiFlow>(new FakeGCDApiFlow(this));
  }

  void SetResponse(const GURL& url, const std::string& response) {
    responses_[url] = response;
  }

 private:
  class FakeGCDApiFlow : public local_discovery::GCDApiFlow {
   public:
    explicit FakeGCDApiFlow(FakeGCDApiFlowFactory* factory)
        : factory_(factory) {}

    virtual ~FakeGCDApiFlow() {}

    virtual void Start(scoped_ptr<Request> request) OVERRIDE {
      std::string response_str = factory_->responses_[request->GetURL()];

      if (response_str == kResponseValueFailure) {
        request->OnGCDAPIFlowError(
            local_discovery::GCDApiFlow::ERROR_MALFORMED_RESPONSE);
        return;
      }

      scoped_ptr<base::Value> response(base::JSONReader::Read(response_str));
      ASSERT_TRUE(response);

      base::DictionaryValue* response_dict;
      ASSERT_TRUE(response->GetAsDictionary(&response_dict));

      request->OnGCDAPIFlowComplete(*response_dict);
    }

   private:
    FakeGCDApiFlowFactory* factory_;
  };

  std::map<GURL /*request url*/, std::string /*response json*/> responses_;
};

class GcdPrivateAPITest : public ExtensionApiTest {
 public:
  GcdPrivateAPITest() : url_fetcher_factory_(&url_fetcher_impl_factory_) {
#if defined(ENABLE_MDNS)
    test_service_discovery_client_ =
        new local_discovery::TestServiceDiscoveryClient();
    test_service_discovery_client_->Start();
#endif  // ENABLE_MDNS
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
  }

#if defined(ENABLE_WIFI_BOOTSTRAPPING)
  virtual void OnCreateWifiManager() {
    wifi_manager_ = wifi_manager_factory_.GetLastCreatedWifiManager();

    EXPECT_CALL(*wifi_manager_, Start());

    EXPECT_CALL(*wifi_manager_,
                RequestNetworkCredentialsInternal("SuccessNetwork"))
        .WillOnce(Invoke(this, &GcdPrivateAPITest::RespondToNetwork));

    EXPECT_CALL(*wifi_manager_,
                RequestNetworkCredentialsInternal("FailureNetwork"))
        .WillOnce(Invoke(this, &GcdPrivateAPITest::RespondToNetwork));
  }

  void RespondToNetwork(const std::string& network) {
    bool success = (network == "SuccessNetwork");

    wifi_manager_->CallRequestNetworkCredentialsCallback(
        success, network, success ? "SuccessPass" : "");
  }
#endif

 protected:
  FakeGCDApiFlowFactory api_flow_factory_;
  net::URLFetcherImplFactory url_fetcher_impl_factory_;
  net::FakeURLFetcherFactory url_fetcher_factory_;

#if defined(ENABLE_MDNS)
  scoped_refptr<local_discovery::TestServiceDiscoveryClient>
      test_service_discovery_client_;
#endif  // ENABLE_MDNS

#if defined(ENABLE_WIFI_BOOTSTRAPPING)
  local_discovery::wifi::MockWifiManagerFactory wifi_manager_factory_;
  local_discovery::wifi::MockWifiManager* wifi_manager_;
#endif
};

IN_PROC_BROWSER_TEST_F(GcdPrivateAPITest, GetCloudList) {
  api_flow_factory_.SetResponse(
      GURL("https://www.google.com/cloudprint/search"), kCloudPrintResponse);

  api_flow_factory_.SetResponse(
      GURL("https://www.googleapis.com/clouddevices/v1/devices"), kGCDResponse);

  EXPECT_TRUE(RunExtensionSubtest("gcd_private/api", "get_cloud_list.html"));
}

IN_PROC_BROWSER_TEST_F(GcdPrivateAPITest, Session) {
  url_fetcher_factory_.SetFakeResponse(GURL("http://1.2.3.4:9090/privet/info"),
                                       kPrivetInfoResponse,
                                       net::HTTP_OK,
                                       net::URLRequestStatus::SUCCESS);

  url_fetcher_factory_.SetFakeResponse(GURL("http://1.2.3.4:9090/privet/ping"),
                                       kPrivetPingResponse,
                                       net::HTTP_OK,
                                       net::URLRequestStatus::SUCCESS);

  EXPECT_TRUE(RunExtensionSubtest("gcd_private/api", "session.html"));
}

#if defined(ENABLE_MDNS)

IN_PROC_BROWSER_TEST_F(GcdPrivateAPITest, AddBefore) {
  test_service_discovery_client_->SimulateReceive(kAnnouncePacket,
                                                  sizeof(kAnnouncePacket));

  EXPECT_TRUE(
      RunExtensionSubtest("gcd_private/api", "receive_new_device.html"));
}

IN_PROC_BROWSER_TEST_F(GcdPrivateAPITest, AddAfter) {
  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&local_discovery::TestServiceDiscoveryClient::SimulateReceive,
                 test_service_discovery_client_,
                 kAnnouncePacket,
                 sizeof(kAnnouncePacket)),
      base::TimeDelta::FromSeconds(1));

  EXPECT_TRUE(
      RunExtensionSubtest("gcd_private/api", "receive_new_device.html"));
}

IN_PROC_BROWSER_TEST_F(GcdPrivateAPITest, AddRemove) {
  test_service_discovery_client_->SimulateReceive(kAnnouncePacket,
                                                  sizeof(kAnnouncePacket));

  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&local_discovery::TestServiceDiscoveryClient::SimulateReceive,
                 test_service_discovery_client_,
                 kGoodbyePacket,
                 sizeof(kGoodbyePacket)),
      base::TimeDelta::FromSeconds(1));

  EXPECT_TRUE(RunExtensionSubtest("gcd_private/api", "remove_device.html"));
}

IN_PROC_BROWSER_TEST_F(GcdPrivateAPITest, SendQuery) {
// TODO(noamsml): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
// See http://crbug.com/177163 for details.
#if !defined(OS_WIN) || defined(NDEBUG)
  EXPECT_CALL(*test_service_discovery_client_,
              OnSendTo(std::string(reinterpret_cast<const char*>(kQueryPacket),
                                   sizeof(kQueryPacket)))).Times(2);
#endif
  EXPECT_TRUE(RunExtensionSubtest("gcd_private/api", "send_query.html"));
}

#endif  // ENABLE_MDNS

#if defined(ENABLE_WIFI_BOOTSTRAPPING)

IN_PROC_BROWSER_TEST_F(GcdPrivateAPITest, WifiMessage) {
  EXPECT_TRUE(RunExtensionSubtest("gcd_private/api", "wifi_message.html"));
}

IN_PROC_BROWSER_TEST_F(GcdPrivateAPITest, WifiPasswords) {
// TODO(noamsml): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
// See http://crbug.com/177163 for details.
#if !defined(OS_WIN) || defined(NDEBUG)
  EXPECT_CALL(wifi_manager_factory_, WifiManagerCreated())
      .WillOnce(Invoke(this, &GcdPrivateAPITest::OnCreateWifiManager));
#endif

  EXPECT_TRUE(RunExtensionSubtest("gcd_private/api", "wifi_password.html"));
}

#endif  // ENABLE_WIFI_BOOTSTRAPPING

}  // namespace
