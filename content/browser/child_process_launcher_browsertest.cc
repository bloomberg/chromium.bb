// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"

namespace content {

typedef ContentBrowserTest ChildProcessLauncherBrowserTest;

class StatsTableBrowserTest : public ChildProcessLauncherBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableStatsTable);
  }
};

IN_PROC_BROWSER_TEST_F(StatsTableBrowserTest, StartWithStatTable) {
  NavigateToURL(shell(), GURL("about:blank"));
}

}  // namespace content
