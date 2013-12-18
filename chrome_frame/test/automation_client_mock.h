// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_AUTOMATION_CLIENT_MOCK_H_
#define CHROME_FRAME_TEST_AUTOMATION_CLIENT_MOCK_H_

#include <windows.h>
#include <string>

#include "base/strings/string_util.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/proxy_factory_mock.h"
#include "chrome_frame/utils.h"
#include "gmock/gmock.h"

using testing::StrictMock;

class MockAutomationProxy : public ChromeFrameAutomationProxy {
 public:
  MOCK_METHOD1(Send, bool(IPC::Message*));
  MOCK_METHOD3(SendAsAsync,
               void(IPC::SyncMessage* msg,
                    SyncMessageReplyDispatcher::SyncMessageCallContext* context,
                    void* key));
  MOCK_METHOD1(CancelAsync, void(void* key));
  MOCK_METHOD1(CreateTabProxy, scoped_refptr<TabProxy>(int handle));
  MOCK_METHOD1(ReleaseTabProxy, void(AutomationHandle handle));
  MOCK_METHOD0(server_version, std::string(void));

  ~MockAutomationProxy() {}
};

struct MockAutomationMessageSender : public AutomationMessageSender {
  virtual bool Send(IPC::Message* msg) {
    return proxy_->Send(msg);
  }

  virtual bool Send(IPC::Message* msg, int timeout_ms) {
    return proxy_->Send(msg);
  }

  void ForwardTo(StrictMock<MockAutomationProxy> *p) {
    proxy_ = p;
  }

  StrictMock<MockAutomationProxy>* proxy_;
};

#endif  // CHROME_FRAME_TEST_AUTOMATION_CLIENT_MOCK_H_
