// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/proxy_launcher.h"

#include "chrome/common/automation_constants.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/ui/ui_test.h"

// Default path of named testing interface.
static const char kInterfacePath[] = "/var/tmp/ChromeTestingInterface";

// NamedProxyLauncher functions

NamedProxyLauncher::NamedProxyLauncher(bool launch_browser,
                                       bool disconnect_on_failure)
    : launch_browser_(launch_browser),
      disconnect_on_failure_(disconnect_on_failure) {
  channel_id_ = kInterfacePath;
}

AutomationProxy* NamedProxyLauncher::CreateAutomationProxy(
    int execution_timeout) const {
  AutomationProxy* proxy = new AutomationProxy(execution_timeout,
                                               disconnect_on_failure_);
  proxy->InitializeChannel(channel_id_, false);
  return proxy;
}

void NamedProxyLauncher::InitializeConnection(UITestBase* ui_test_base) const {
  if (launch_browser_) {
    // Set up IPC testing interface as a client.
    ui_test_base->LaunchBrowser();

    // Wait for browser to be ready for connections.
    struct stat file_info;
    while (stat(kInterfacePath, &file_info))
      PlatformThread::Sleep(automation::kSleepTime);
  }

  ui_test_base->ConnectToRunningBrowser();
}

std::string NamedProxyLauncher::PrefixedChannelID() const {
  std::string channel_id;
  channel_id.append(automation::kNamedInterfacePrefix).append(channel_id_);
  return channel_id;
}

// AnonymousProxyLauncher functions

AnonymousProxyLauncher::AnonymousProxyLauncher(bool disconnect_on_failure)
    : disconnect_on_failure_(disconnect_on_failure) {
  channel_id_ = AutomationProxy::GenerateChannelID();
}

AutomationProxy* AnonymousProxyLauncher::CreateAutomationProxy(
    int execution_timeout) const {
  AutomationProxy* proxy = new AutomationProxy(execution_timeout,
                                               disconnect_on_failure_);
  proxy->InitializeChannel(channel_id_, true);
  return proxy;
}

void AnonymousProxyLauncher::InitializeConnection(
    UITestBase* ui_test_base) const {
  ui_test_base->LaunchBrowserAndServer();
}

std::string AnonymousProxyLauncher::PrefixedChannelID() const {
  return channel_id_;
}

