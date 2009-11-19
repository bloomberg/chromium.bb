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

class TabProxy;
class ExternalTabUITestMockClient;

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

// Base class for automation proxy testing.
class AutomationProxyVisibleTest : public UITest {
 protected:
  AutomationProxyVisibleTest() {
    show_window_ = true;
  }
};

// Automation proxy UITest that allows tests to override the automation
// proxy used by the UITest base class.
template <class AutomationProxyClass>
class CustomAutomationProxyTest : public AutomationProxyVisibleTest {
 protected:
  CustomAutomationProxyTest() {
  }

  // Override UITest's CreateAutomationProxy to provide our the unit test
  // with our special implementation of AutomationProxy.
  // This function is called from within UITest::LaunchBrowserAndServer.
  virtual AutomationProxy* CreateAutomationProxy(int execution_timeout) {
    AutomationProxyClass* proxy = new AutomationProxyClass(execution_timeout);
    return proxy;
  }
};

// A single-use AutomationProxy implementation that's good
// for a single navigation and a single ForwardMessageToExternalHost
// message.  Once the ForwardMessageToExternalHost message is received
// the class posts a quit message to the thread on which the message
// was received.
class AutomationProxyForExternalTab : public AutomationProxy {
 public:
  // Allows us to reuse this mock for multiple tests. This is done
  // by setting a state to trigger posting of Quit message to the
  // wait loop.
  enum QuitAfter {
    QUIT_INVALID,
    QUIT_AFTER_NAVIGATION,
    QUIT_AFTER_MESSAGE,
  };

  explicit AutomationProxyForExternalTab(int execution_timeout);
  ~AutomationProxyForExternalTab();

  int messages_received() const {
    return messages_received_;
  }

  const std::string& message() const {
    return message_;
  }

  const std::string& origin() const {
    return origin_;
  }

  const std::string& target() const {
    return target_;
  }

  // Creates and sisplays a top-level window, that can be used as a parent
  // to the external tab.window.
  gfx::NativeWindow CreateHostWindow();
  scoped_refptr<TabProxy> CreateTabWithHostWindow(bool is_incognito,
      const GURL& initial_url, gfx::NativeWindow* container_wnd,
      gfx::NativeWindow* tab_wnd);
  void DestroyHostWindow();

  // Wait for the event to happen or timeout
  bool WaitForNavigation(int timeout_ms);
  bool WaitForMessage(int timeout_ms);
  bool WaitForTabCleanup(TabProxy* tab, int timeout_ms);

  // Enters a message loop that processes window messages as well
  // as calling MessageLoop::current()->RunAllPending() to process any
  // incoming IPC messages. The timeout_ms parameter is the maximum
  // time the loop will run. To end the loop earlier, post a quit message to
  // the thread.
  bool RunMessageLoop(int timeout_ms, gfx::NativeWindow window_to_monitor);

 protected:
#if defined(OS_WIN)
  static const int kQuitLoopMessage = WM_APP + 11;
  // Quit the message loop
  void QuitLoop() {
    DCHECK(IsWindow(host_window_));
    // We could post WM_QUIT but lets keep it out of accidental usage
    // by anyone else peeking it.
    PostMessage(host_window_, kQuitLoopMessage, 0, 0);
  }
#endif  // defined(OS_WIN)

  // Internal state to flag posting of a quit message to the loop
  void set_quit_after(QuitAfter q) {
    quit_after_ = q;
  }

  virtual void OnMessageReceived(const IPC::Message& msg);

  void OnDidNavigate(int tab_handle, const IPC::NavigationInfo& nav_info);
  void OnForwardMessageToExternalHost(int handle,
                                      const std::string& message,
                                      const std::string& origin,
                                      const std::string& target);

 protected:
  bool navigate_complete_;
  int messages_received_;
  std::string message_, origin_, target_;
  QuitAfter quit_after_;
  const wchar_t* host_window_class_;
  gfx::NativeWindow host_window_;
};

// A test harness for testing external tabs.
typedef CustomAutomationProxyTest<AutomationProxyForExternalTab>
    ExternalTabTestType;

#endif  // CHROME_TEST_AUTOMATION_AUTOMATION_PROXY_UITEST_H_
