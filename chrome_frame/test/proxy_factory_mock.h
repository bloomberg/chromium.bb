// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_TEST_PROXY_FACTORY_MOCK_H_
#define CHROME_FRAME_TEST_PROXY_FACTORY_MOCK_H_

#include <windows.h>
#include <string>

#include "gmock/gmock.h"
#include "chrome_frame/chrome_frame_automation.h"

struct LaunchDelegateMock : public LaunchDelegate {
  MOCK_METHOD2(LaunchComplete, void(ChromeFrameAutomationProxy*,
      AutomationLaunchResult));
  MOCK_METHOD0(AutomationServerDied, void());
};

class MockProxyFactory : public ProxyFactory {
 public:
  MOCK_METHOD3(GetAutomationServer,
      void (LaunchDelegate*,  // NOLINT
            ChromeFrameLaunchParams* params,
            void** automation_server_id));
  MOCK_METHOD2(ReleaseAutomationServer, bool(void* server_id,
                                             LaunchDelegate* delegate));

  MockProxyFactory() : thread_("mock factory worker") {
    thread_.Start();
    loop_ = thread_.message_loop();
  }

  // Fake implementation
  void GetServerImpl(ChromeFrameAutomationProxy* pxy,
                     void* proxy_id,
                     AutomationLaunchResult result,
                     LaunchDelegate* d,
                     ChromeFrameLaunchParams* params,
                     void** automation_server_id);

  base::Thread thread_;
  MessageLoop* loop_;
};


#endif  // CHROME_FRAME_TEST_PROXY_FACTORY_MOCK_H_

