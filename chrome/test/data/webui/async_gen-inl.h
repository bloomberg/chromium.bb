// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/ui/webui/web_ui_browsertest.h"
#include "testing/gmock/include/gmock/gmock.h"

// C++ support class for javascript-generated asynchronous tests.
class WebUIBrowserAsyncGenTest : public WebUIBrowserTest {
 public:
  WebUIBrowserAsyncGenTest();
  virtual ~WebUIBrowserAsyncGenTest();

 protected:
  class AsyncWebUIMessageHandler : public WebUIMessageHandler {
   public:
    AsyncWebUIMessageHandler();
    ~AsyncWebUIMessageHandler();

    MOCK_METHOD1(HandleTornDown, void(const base::ListValue*));

   private:
    void HandleCallJS(const base::ListValue* list_value) {
      std::string call_js;
      ASSERT_TRUE(list_value->GetString(0, &call_js));
      web_ui_->CallJavascriptFunction(call_js);
    }

    virtual void RegisterMessages() OVERRIDE {
      web_ui_->RegisterMessageCallback("callJS", NewCallback(
          this, &AsyncWebUIMessageHandler::HandleCallJS));
      web_ui_->RegisterMessageCallback("tornDown", NewCallback(
          this, &AsyncWebUIMessageHandler::HandleTornDown));
    }
  };

  // Handler for this test fixture.
  ::testing::StrictMock<AsyncWebUIMessageHandler> message_handler_;

 private:
  // Provide this object's handler.
  virtual WebUIMessageHandler* GetMockMessageHandler() OVERRIDE {
    return &message_handler_;
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    WebUIBrowserTest::SetUpOnMainThread();
    EXPECT_CALL(message_handler_, HandleTornDown(::testing::_));
  }

  DISALLOW_COPY_AND_ASSIGN(WebUIBrowserAsyncGenTest);
};
