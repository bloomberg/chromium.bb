// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_UITEST_H_
#define CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_UITEST_H_

#include <string>

#include "app/gfx/native_widget_types.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "base/time.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"

class TabProxy;
class ExternalTabUITestMockClient;

// Base class for automation proxy testing.
class AutomationProxyVisibleTest : public UITest {
 protected:
  AutomationProxyVisibleTest() {
    show_window_ = true;
  }
};

// Used to implement external tab UI tests.
//
// We have to derive from AutomationProxy in order to hook up
// OnMessageReceived callbacks.
class ExternalTabUITestMockClient : public AutomationProxy {
 public:
  explicit ExternalTabUITestMockClient(int execution_timeout);
  ~ExternalTabUITestMockClient() {
    EXPECT_TRUE(host_window_ == NULL);
  }

  MOCK_METHOD2(OnDidNavigate, void(int tab_handle,
      const IPC::NavigationInfo& nav_info));
  MOCK_METHOD4(OnForwardMessageToExternalHost, void(int handle,
      const std::string& message, const std::string& origin,
      const std::string& target));
  MOCK_METHOD3(OnRequestStart, void(int tab_handle, int request_id,
      const IPC::AutomationURLRequest& request));
  MOCK_METHOD3(OnRequestRead, void(int tab_handle, int request_id,
      int bytes_to_read));
  MOCK_METHOD3(OnRequestEnd, void(int tab_handle, int request_id,
      const URLRequestStatus& status));
  MOCK_METHOD3(OnSetCookieAsync, void(int tab_handle, const GURL& url,
      const std::string& cookie));

  MOCK_METHOD1(HandleClosed, void(int handle));

  // Action helpers for OnRequest* incoming messages. Create the message and
  // delegate sending to the base class. Apparently we do not have wrappers
  // in AutomationProxy for these messages.
  void ReplyStarted(const IPC::AutomationURLResponse* response,
                    int tab_handle, int request_id);
  void ReplyData(const std::string* data, int tab_handle, int request_id);
  void ReplyEOF(int tab_handle, int request_id);
  void Reply404(int tab_handle, int request_id);

  // Test setup helpers
  scoped_refptr<TabProxy> CreateHostWindowAndTab(
      const IPC::ExternalTabSettings& settings);
  scoped_refptr<TabProxy> CreateTabWithUrl(const GURL& initial_url);

  // Destroys the host window.
  void DestroyHostWindow();
  // Returns true if the host window exists.
  bool HostWindowExists();

  static const IPC::ExternalTabSettings default_settings;
 protected:
   gfx::NativeWindow host_window_;

  // Simple dispatcher to above OnXXX methods.
  virtual void OnMessageReceived(const IPC::Message& msg);
  virtual void InvalidateHandle(const IPC::Message& message);
};

// Base your external tab UI tests on this.
class ExternalTabUITest : public UITest {
 public:
  // Override UITest's CreateAutomationProxy to provide the unit test
  // with our special implementation of AutomationProxy.
  // This function is called from within UITest::LaunchBrowserAndServer.
  virtual AutomationProxy* CreateAutomationProxy(int execution_timeout);
 protected:
  // Filtered Inet will override automation callbacks for network resources.
  virtual bool ShouldFilterInet() {
    return false;
  }
  ExternalTabUITestMockClient* mock_;
};

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_UITEST_H_
