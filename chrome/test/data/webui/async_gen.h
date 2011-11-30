// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_TEST_DATA_WEBUI_ASYNC_GEN_H_
#define CHROME_TEST_DATA_WEBUI_ASYNC_GEN_H_
#pragma once

#include "chrome/browser/ui/webui/web_ui_browsertest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class ListValue;
}  // namespace base

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

    MOCK_METHOD1(HandleTearDown, void(const base::ListValue*));

   private:
    void HandleCallJS(const base::ListValue* list_value);

    // WebUIMessageHandler implementation.
    virtual void RegisterMessages() OVERRIDE;
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
    EXPECT_CALL(message_handler_, HandleTearDown(::testing::_));
  }

  DISALLOW_COPY_AND_ASSIGN(WebUIBrowserAsyncGenTest);
};

#endif  // CHROME_TEST_DATA_WEBUI_ASYNC_GEN_H_
