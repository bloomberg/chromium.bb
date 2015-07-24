// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_WEBRTC_DEVTOOLS_BRIDGE_CLIENT_BROWSERTEST_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_WEBRTC_DEVTOOLS_BRIDGE_CLIENT_BROWSERTEST_H_

#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/test/base/web_ui_browser_test.h"

class ProfileOAuth2TokenService;

namespace content {
class WebUIMessageHandler;
}

class DevToolsBridgeClientBrowserTest : public WebUIBrowserTest {
 public:
  DevToolsBridgeClientBrowserTest();
  ~DevToolsBridgeClientBrowserTest() override;

  // InProcessBrowserTest overrides.
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  content::WebUIMessageHandler* GetMockMessageHandler() override;

 private:
  class DevToolsBridgeClientMock;
  class GCDApiFlowMock;
  class MessageHandler;

  scoped_ptr<FakeSigninManagerForTesting> fake_signin_manager_;
  scoped_ptr<ProfileOAuth2TokenService> fake_token_service_;
  base::WeakPtr<DevToolsBridgeClientMock> client_mock_;
  scoped_ptr<MessageHandler> handler_;
  std::map<int, GCDApiFlowMock*> flows_;
  int last_flow_id_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_WEBRTC_DEVTOOLS_BRIDGE_CLIENT_BROWSERTEST_H_
