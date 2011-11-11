// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_TEST_DATA_WEBUI_CHROME_SEND_BROWSERTEST_H_
#define CHROME_TEST_DATA_WEBUI_CHROME_SEND_BROWSERTEST_H_
#pragma once

#include "chrome/browser/ui/webui/web_ui_browsertest.h"
#include "testing/gmock/include/gmock/gmock.h"

// Test fixture for testing chrome.send. This class registers the "checkSend"
// message, but should NOT receive it.
class ChromeSendWebUITest : public WebUIBrowserTest {
 public:
  ChromeSendWebUITest();
  virtual ~ChromeSendWebUITest();

  // Mocked message handler class to register expects using gmock framework.
  class ChromeSendWebUIMessageHandler : public WebUIMessageHandler {
   public:
    ChromeSendWebUIMessageHandler();
    ~ChromeSendWebUIMessageHandler();

    MOCK_METHOD1(HandleCheckSend, void(const base::ListValue*));

   private:
    virtual void RegisterMessages() OVERRIDE;
  };


 protected:
  // Strict mock will fail when unexpected chrome.send messages are received.
  ::testing::StrictMock<ChromeSendWebUIMessageHandler> message_handler_;

 private:
  virtual WebUIMessageHandler* GetMockMessageHandler() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromeSendWebUITest);
};

// Test fixture for verifying chrome.send messages are passed through. This
// class DOES expect to receive the "checkSend" message.
class ChromeSendPassthroughWebUITest : public ChromeSendWebUITest {
 public:
  ChromeSendPassthroughWebUITest();
  virtual ~ChromeSendPassthroughWebUITest();

 private:
  virtual void SetUpOnMainThread() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromeSendPassthroughWebUITest);
};

#endif  // CHROME_TEST_DATA_WEBUI_CHROME_SEND_BROWSERTEST_H_
