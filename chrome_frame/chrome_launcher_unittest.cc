// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/chrome_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Utility class to disable logging.  Required to disable DCHECKs that some
// of our tests would otherwise trigger.
class LogDisabler {
 public:
  LogDisabler() {
    initial_log_level_ = logging::GetMinLogLevel();
    logging::SetMinLogLevel(logging::LOG_FATAL + 1);
  }

  ~LogDisabler() {
    logging::SetMinLogLevel(initial_log_level_);
  }

 private:
  int initial_log_level_;
};

}  // namespace

TEST(ChromeLauncher, SanitizeCommandLine) {
  CommandLine bad(L"dummy.exe");
  bad.AppendSwitch(switches::kDisableMetrics);  // in whitelist
  bad.AppendSwitchWithValue(switches::kLoadExtension, L"foo");  // in whitelist
  bad.AppendSwitch("no-such-switch");  // does not exist
  bad.AppendSwitch(switches::kHomePage);  // exists but not in whitelist

  LogDisabler no_dchecks;

  CommandLine sanitized(L"dumbo.exe");
  chrome_launcher::SanitizeCommandLine(bad, &sanitized);
  EXPECT_TRUE(sanitized.HasSwitch(switches::kDisableMetrics));
  EXPECT_FALSE(sanitized.HasSwitch(switches::kLoadExtension));
  EXPECT_FALSE(sanitized.HasSwitch("no-such-switch"));
  EXPECT_FALSE(sanitized.HasSwitch(switches::kHomePage));
}

