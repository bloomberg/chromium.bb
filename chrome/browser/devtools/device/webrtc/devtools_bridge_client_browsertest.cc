// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/webrtc/devtools_bridge_client_browsertest.h"

#include "chrome/browser/devtools/device/webrtc/devtools_bridge_client.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/web_ui_message_handler.h"

class DevToolsBridgeClientBrowserTest::GCDApiFlowMock
    : public local_discovery::GCDApiFlow {
 public:
  explicit GCDApiFlowMock(DevToolsBridgeClientBrowserTest* test)
      : test_(test), id_(++test->last_flow_id_) {
    test_->flows_[id_] = this;
  }

  ~GCDApiFlowMock() override { test_->flows_.erase(id_); }

  // Passes request's data to the JS test. Result will be passed back
  // in MessageHandler::Response.
  void Start(scoped_ptr<Request> request) override {
    request_ = request.Pass();

    std::string type;
    std::string data;
    request_->GetUploadData(&type, &data);

    ScopedVector<const base::Value> params;
    params.push_back(new base::FundamentalValue(id_));
    params.push_back(new base::StringValue(request_->GetURL().spec()));
    params.push_back(new base::StringValue(data));

    test_->RunJavascriptFunction("callbacks.gcdApiRequest", params);
  }

  void Respond(const base::DictionaryValue* response) {
    if (request_.get())
      request_->OnGCDAPIFlowComplete(*response);
  }

 private:
  DevToolsBridgeClientBrowserTest* const test_;
  const int id_;
  scoped_ptr<Request> request_;
};

class DevToolsBridgeClientBrowserTest::DevToolsBridgeClientMock
    : public DevToolsBridgeClient,
      public base::SupportsWeakPtr<DevToolsBridgeClientMock> {
 public:
  explicit DevToolsBridgeClientMock(DevToolsBridgeClientBrowserTest* test)
      : DevToolsBridgeClient(test->browser()->profile(),
                             test->fake_signin_manager_.get(),
                             test->fake_token_service_.get()),
        test_(test) {}

  ~DevToolsBridgeClientMock() override {}

  void DocumentOnLoadCompletedInMainFrame() override {
    DevToolsBridgeClient::DocumentOnLoadCompletedInMainFrame();

    test_->RunJavascriptFunction("callbacks.workerLoaded");
  }

  void OnBrowserListUpdatedForTests() override {
    int count = static_cast<int>(browsers().size());
    test_->RunJavascriptFunction("callbacks.browserListUpdated",
                                 new base::FundamentalValue(count));
  }

  scoped_ptr<local_discovery::GCDApiFlow> CreateGCDApiFlow() override {
    return make_scoped_ptr(new GCDApiFlowMock(test_));
  }

  void GoogleSigninSucceeded() {
    // This username is checked on Chrome OS.
    const std::string username = "stub-user@example.com";
    test_->fake_signin_manager_->SetAuthenticatedUsername(username);
    identity_provider().GoogleSigninSucceeded("test_account", username,
                                              "testing");
  }

 private:
  DevToolsBridgeClientBrowserTest* const test_;
};

class DevToolsBridgeClientBrowserTest::MessageHandler
    : public content::WebUIMessageHandler {
 public:
  explicit MessageHandler(DevToolsBridgeClientBrowserTest* test)
      : test_(test) {}

  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "signIn", base::Bind(&MessageHandler::SignIn, base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "gcdApiResponse",
        base::Bind(&MessageHandler::GCDApiResponse, base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "queryDevices",
        base::Bind(&MessageHandler::QueryDevices, base::Unretained(this)));
  }

  void SignIn(const base::ListValue*) {
    if (test_->client_mock_.get())
      test_->client_mock_->GoogleSigninSucceeded();
    test_->fake_token_service_->UpdateCredentials("test_user@gmail.com",
                                                  "token");
  }

  void GCDApiResponse(const base::ListValue* params) {
    CHECK(params->GetSize() >= 2);
    int id;
    const base::DictionaryValue* response;
    CHECK(params->GetInteger(0, &id));
    CHECK(params->GetDictionary(1, &response));

    auto flow = test_->flows_.find(id);
    CHECK(test_->flows_.end() != flow);
    flow->second->Respond(response);
  }

  void QueryDevices(const base::ListValue*) {
    DevToolsBridgeClient::GetDevices(test_->client_mock_);
  }

 private:
  DevToolsBridgeClientBrowserTest* const test_;
};

DevToolsBridgeClientBrowserTest::DevToolsBridgeClientBrowserTest()
    : last_flow_id_(0) {
}

DevToolsBridgeClientBrowserTest::~DevToolsBridgeClientBrowserTest() {
  DCHECK(flows_.empty());
}

void DevToolsBridgeClientBrowserTest::SetUpOnMainThread() {
  WebUIBrowserTest::SetUpOnMainThread();

  DCHECK(browser()->profile());
  fake_signin_manager_.reset(
      new FakeSigninManagerForTesting(browser()->profile()));
  fake_token_service_.reset(new FakeProfileOAuth2TokenService());
  client_mock_ = (new DevToolsBridgeClientMock(this))->AsWeakPtr();
}

void DevToolsBridgeClientBrowserTest::TearDownOnMainThread() {
  if (client_mock_.get())
    client_mock_->DeleteSelf();
  fake_token_service_.reset();
  fake_signin_manager_.reset();
  WebUIBrowserTest::TearDownOnMainThread();
}

content::WebUIMessageHandler*
DevToolsBridgeClientBrowserTest::GetMockMessageHandler() {
  if (!handler_.get())
    handler_.reset(new MessageHandler(this));
  return handler_.get();
}
