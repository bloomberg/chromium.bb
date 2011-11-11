// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
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
    virtual void RegisterMessages() OVERRIDE {
      web_ui_->RegisterMessageCallback(
          "checkSend",
          base::Bind(&ChromeSendWebUIMessageHandler::HandleCheckSend,
                     base::Unretained(this)));
    }
  };

 protected:
  // Strict mock will fail when unexpected chrome.send messages are received.
  ::testing::StrictMock<ChromeSendWebUIMessageHandler> message_handler_;

 private:
  virtual WebUIMessageHandler* GetMockMessageHandler() OVERRIDE {
    return &message_handler_;
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeSendWebUITest);
};

// Test fixture for verifying chrome.send messages are passed through. This
// class DOES expect to receive the "checkSend" message.
class ChromeSendPassthroughWebUITest : public ChromeSendWebUITest {
 public:
  ChromeSendPassthroughWebUITest();
  virtual ~ChromeSendPassthroughWebUITest();

  virtual void SetUpOnMainThread() OVERRIDE {
    ChromeSendWebUITest::SetUpOnMainThread();
    EXPECT_CALL(message_handler_, HandleCheckSend(::testing::_));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSendPassthroughWebUITest);
};
