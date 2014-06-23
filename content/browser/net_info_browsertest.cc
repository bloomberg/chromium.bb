// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_factory.h"

class NetInfoBrowserTest : public content::ContentBrowserTest {
 protected:
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE {
    // TODO(jkarlin): Once NetInfo is enabled by default remove this switch.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

#if defined(OS_CHROMEOS)
  virtual void SetUp() OVERRIDE {
    // ChromeOS's NetworkChangeNotifier isn't known to content and therefore
    // doesn't get created in content_browsertests. Insert a mock
    // NetworkChangeNotifier.
    net::NetworkChangeNotifier::CreateMock();
    content::ContentBrowserTest::SetUp();
  }
#endif

  virtual void SetUpOnMainThread() OVERRIDE {
    net::NetworkChangeNotifier::SetTestNotificationsOnly(true);
    base::RunLoop().RunUntilIdle();
  }

  static void SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType type) {
    net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
        type);
    base::RunLoop().RunUntilIdle();
  }

  std::string RunScriptExtractString(const std::string& script) {
    std::string data;
    EXPECT_TRUE(
        ExecuteScriptAndExtractString(shell()->web_contents(), script, &data));
    return data;
  }

  bool RunScriptExtractBool(const std::string& script) {
    bool data;
    EXPECT_TRUE(
        ExecuteScriptAndExtractBool(shell()->web_contents(), script, &data));
    return data;
  }
};

// Make sure that type changes in the browser make their way to
// navigator.connection.type.
IN_PROC_BROWSER_TEST_F(NetInfoBrowserTest, NetworkChangePlumbsToNavigator) {
  NavigateToURL(shell(), content::GetTestUrl("", "net_info.html"));
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_EQ("wifi", RunScriptExtractString("getType()"));
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  EXPECT_EQ("ethernet", RunScriptExtractString("getType()"));
}

// Make sure that type changes in the browser make their way to
// navigator.isOnline.
IN_PROC_BROWSER_TEST_F(NetInfoBrowserTest, IsOnline) {
  NavigateToURL(shell(), content::GetTestUrl("", "net_info.html"));
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  EXPECT_TRUE(RunScriptExtractBool("getOnLine()"));
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_FALSE(RunScriptExtractBool("getOnLine()"));
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  EXPECT_TRUE(RunScriptExtractBool("getOnLine()"));
}
