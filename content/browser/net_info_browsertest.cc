// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_factory.h"

namespace content {

class NetInfoBrowserTest : public content::ContentBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(jkarlin): Once NetInfo is enabled on all platforms remove this
    // switch.
    command_line->AppendSwitch(switches::kEnableNetworkInformation);

    // TODO(jkarlin): Remove this once downlinkMax is no longer
    // experimental.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUp() override {
    net::NetworkChangeNotifier::SetTestNotificationsOnly(true);

#if defined(OS_CHROMEOS)
    // ChromeOS's NetworkChangeNotifier isn't known to content and therefore
    // doesn't get created in content_browsertests. Insert a mock
    // NetworkChangeNotifier.
    net::NetworkChangeNotifier::CreateMock();
#endif

    content::ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    base::RunLoop().RunUntilIdle();
  }

  static void SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType type,
      net::NetworkChangeNotifier::ConnectionSubtype subtype) {
    net::NetworkChangeNotifier::NotifyObserversOfMaxBandwidthChangeForTests(
        net::NetworkChangeNotifier::GetMaxBandwidthForConnectionSubtype(
            subtype),
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

  double RunScriptExtractDouble(const std::string& script) {
    double data = 0.0;
    EXPECT_TRUE(base::StringToDouble(RunScriptExtractString(script), &data));
    return data;
  }
};

// Make sure that type changes in the browser make their way to
// navigator.connection.type.
IN_PROC_BROWSER_TEST_F(NetInfoBrowserTest, NetworkChangePlumbsToNavigator) {
  NavigateToURL(shell(), content::GetTestUrl("", "net_info.html"));
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI,
                    net::NetworkChangeNotifier::SUBTYPE_WIFI_N);
  EXPECT_EQ("wifi", RunScriptExtractString("getType()"));
  EXPECT_EQ(net::NetworkChangeNotifier::GetMaxBandwidthForConnectionSubtype(
                net::NetworkChangeNotifier::SUBTYPE_WIFI_N),
            RunScriptExtractDouble("getDownlinkMax()"));

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_ETHERNET,
                    net::NetworkChangeNotifier::SUBTYPE_GIGABIT_ETHERNET);
  EXPECT_EQ("ethernet", RunScriptExtractString("getType()"));
  EXPECT_EQ(net::NetworkChangeNotifier::GetMaxBandwidthForConnectionSubtype(
                net::NetworkChangeNotifier::SUBTYPE_GIGABIT_ETHERNET),
            RunScriptExtractDouble("getDownlinkMax()"));
}

// Make sure that type changes in the browser make their way to
// navigator.isOnline.
IN_PROC_BROWSER_TEST_F(NetInfoBrowserTest, IsOnline) {
  NavigateToURL(shell(), content::GetTestUrl("", "net_info.html"));
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_ETHERNET,
                    net::NetworkChangeNotifier::SUBTYPE_GIGABIT_ETHERNET);
  EXPECT_TRUE(RunScriptExtractBool("getOnLine()"));
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE,
                    net::NetworkChangeNotifier::SUBTYPE_NONE);
  EXPECT_FALSE(RunScriptExtractBool("getOnLine()"));
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI,
                    net::NetworkChangeNotifier::SUBTYPE_WIFI_N);
  EXPECT_TRUE(RunScriptExtractBool("getOnLine()"));
}

}  // namespace content
