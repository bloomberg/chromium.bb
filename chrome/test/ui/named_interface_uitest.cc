// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/proxy_launcher.h"
#include "chrome/test/base/test_switches.h"

// The named testing interface enables the use of a named socket for controlling
// the browser. This eliminates the dependency that the browser must be forked
// from the controlling process.
namespace {

class NamedInterfaceTest : public UITest {
 public:
  NamedInterfaceTest() {
    show_window_ = true;
  }

  virtual ProxyLauncher *CreateProxyLauncher() {
    std::string channel_id =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kTestingChannel);
    if (channel_id.empty())
      channel_id = ProxyLauncher::kDefaultInterfaceId;

    return new NamedProxyLauncher(channel_id, true, true);
  }
};

// Basic sanity test for named testing interface which
// launches a browser instance that uses a named socket, then
// sends it some commands to open some tabs over that socket.
TEST_F(NamedInterfaceTest, BasicNamedInterface) {
  scoped_refptr<BrowserProxy> browser_proxy(
      automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser_proxy.get());

  for (int i = 0; i < 10; ++i)
    ASSERT_TRUE(browser_proxy->AppendTab(GURL(chrome::kAboutBlankURL)));
}

// TODO(dtu): crosbug.com/8514: Write a test that makes sure you can disconnect,
//            then reconnect with a new connection and continue automation.

}  // namespace
