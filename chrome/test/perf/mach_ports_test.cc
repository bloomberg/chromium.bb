// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/string_util.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/perf/perf_test.h"
#include "chrome/test/ui/ui_perf_test.h"
#include "net/test/spawned_test_server.h"

namespace {

// This test spawns a new browser and counts the number of open Mach ports in
// the browser process. It navigates tabs and closes them, repeatedly measuring
// the number of open ports. This is used to protect against leaking Mach ports,
// which was the source of <http://crbug.com/105513>.
class MachPortsTest : public UIPerfTest {
 public:
  MachPortsTest()
      : server_(net::SpawnedTestServer::TYPE_HTTP,
                net::SpawnedTestServer::kLocalhost,
                base::FilePath(FILE_PATH_LITERAL("data/mach_ports/moz"))) {
  }

  virtual void SetUp() OVERRIDE {
    UIPerfTest::SetUp();

    ASSERT_TRUE(server_.Start());
  }

  virtual void TearDown() OVERRIDE {
    std::string ports;
    for (std::vector<int>::iterator it = port_counts_.begin();
         it != port_counts_.end(); ++it) {
      base::StringAppendF(&ports, "%d,", *it);
    }
    perf_test::PrintResultList("mach_ports", "", "", ports, "ports", true);

    UIPerfTest::TearDown();
  }

  // Gets the browser's current number of Mach ports and records it.
  void RecordPortCount() {
    int port_count = 0;
    ASSERT_TRUE(automation()->Send(
        new AutomationMsg_GetMachPortCount(&port_count)));
    port_counts_.push_back(port_count);
  }

  // Adds a tab from the page cycler data at the specified domain.
  bool AddTab(scoped_refptr<BrowserProxy> browser, const std::string& domain) {
    GURL url = server_.GetURL("files/" + domain + "/").Resolve("?skip");
    return browser->AppendTab(url);
  }

 private:
  net::SpawnedTestServer server_;
  std::vector<int> port_counts_;
};

TEST_F(MachPortsTest, GetCounts) {
  ASSERT_EQ(AUTOMATION_SUCCESS, automation()->WaitForAppLaunch());

  // Record startup number.
  RecordPortCount();

  // Create a browser and open a few tabs.
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());

  EXPECT_TRUE(AddTab(browser, "www.google.com"));
  RecordPortCount();

  EXPECT_TRUE(AddTab(browser, "www.cnn.com"));
  RecordPortCount();

  EXPECT_TRUE(AddTab(browser, "www.nytimes.com"));
  RecordPortCount();

  int tab_count = 0;
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  EXPECT_EQ(4, tab_count);  // Also count about:blank.

  // Close each tab, recording the number of ports after each. Do not close the
  // last tab, which is about:blank because it will be closed by the proxy.
  for (int i = 0; i < tab_count - 1; ++i) {
    scoped_refptr<TabProxy> tab(browser->GetActiveTab());
    ASSERT_TRUE(tab.get());

    EXPECT_TRUE(tab->Close(true));
    RecordPortCount();
  }
}

}  // namespace
