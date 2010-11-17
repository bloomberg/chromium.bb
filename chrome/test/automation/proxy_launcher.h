// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_PROXY_LAUNCHER_H_
#define CHROME_TEST_AUTOMATION_PROXY_LAUNCHER_H_

#include <string>

#include "base/basictypes.h"

class AutomationProxy;
class UITestBase;

// Subclass from this class to use a different implementation of AutomationProxy
// or to use different channel IDs inside a class that derives from UITest.
class ProxyLauncher {
 public:
  ProxyLauncher() {}
  virtual ~ProxyLauncher() {}

  // Creates an automation proxy.
  virtual AutomationProxy* CreateAutomationProxy(
      int execution_timeout) const = 0;

  // Launches the browser if needed and establishes a connection
  // connection with it using the specified UITestBase.
  virtual void InitializeConnection(UITestBase* ui_test_base) const = 0;

  // Returns the automation proxy's channel with any prefixes prepended,
  // for passing as a command line parameter over to the browser.
  virtual std::string PrefixedChannelID() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyLauncher);
};

// Uses an automation proxy that communicates over a named socket.
// This is used if you want to connect an AutomationProxy
// to a browser process that is already running.
// The channel id of the proxy is a constant specified by kInterfacePath.
class NamedProxyLauncher : public ProxyLauncher {
 public:
  // If launch_browser is true, launches Chrome with named interface enabled.
  // Otherwise, there should be an existing instance the proxy can connect to.
  NamedProxyLauncher(bool launch_browser, bool disconnect_on_failure);

  virtual AutomationProxy* CreateAutomationProxy(int execution_timeout) const;
  virtual void InitializeConnection(UITestBase* ui_test_base) const;
  virtual std::string PrefixedChannelID() const;

 protected:
  std::string channel_id_;      // Channel id of automation proxy.
  bool launch_browser_;         // True if we should launch the browser too.
  bool disconnect_on_failure_;  // True if we disconnect on IPC channel failure.

 private:
  DISALLOW_COPY_AND_ASSIGN(NamedProxyLauncher);
};

// Uses an automation proxy that communicates over an anonymous socket.
class AnonymousProxyLauncher : public ProxyLauncher {
 public:
  explicit AnonymousProxyLauncher(bool disconnect_on_failure);
  virtual AutomationProxy* CreateAutomationProxy(int execution_timeout) const;
  virtual void InitializeConnection(UITestBase* ui_test_base) const;
  virtual std::string PrefixedChannelID() const;

 protected:
  std::string channel_id_;      // Channel id of automation proxy.
  bool disconnect_on_failure_;  // True if we disconnect on IPC channel failure.

 private:
  DISALLOW_COPY_AND_ASSIGN(AnonymousProxyLauncher);
};

#endif  // CHROME_TEST_AUTOMATION_PROXY_LAUNCHER_H_

