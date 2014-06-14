// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "chrome/browser/extensions/api/gcd_private/gcd_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/api/mdns.h"
#include "extensions/common/switches.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace api = extensions::api;

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
  GcdPrivateAPITest() {}

 protected:
  FakeGCDApiFlowFactory api_flow_factory_;
};

IN_PROC_BROWSER_TEST_F(GcdPrivateAPITest, GetCloudList) {
  api_flow_factory_.SetResponse(
      GURL("https://www.google.com/cloudprint/search"), kCloudPrintResponse);

  api_flow_factory_.SetResponse(
      GURL("https://www.googleapis.com/clouddevices/v1/devices"), kGCDResponse);

  EXPECT_TRUE(RunExtensionSubtest("gcd_private/api", "get_cloud_list.html"));
}

}  // namespace
