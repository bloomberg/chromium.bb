// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/main_function_params.h"
#include "net/socket/client_socket_pool_base.h"
#include "testing/gtest/include/gtest/gtest.h"

class BrowserMainTest : public testing::Test {
 public:
  BrowserMainTest()
      : command_line_(CommandLine::NO_PROGRAM) {
    ChromeBrowserMainParts::disable_enforcing_cookie_policies_for_tests_ = true;
  }

 protected:
  CommandLine command_line_;
};

TEST_F(BrowserMainTest, WarmConnectionFieldTrial_WarmestSocket) {
  command_line_.AppendSwitchASCII(switches::kSocketReusePolicy, "0");

  scoped_ptr<content::MainFunctionParams> params(
      new content::MainFunctionParams(command_line_));
  scoped_ptr<content::BrowserMainParts> browser_main_parts(
      content::GetContentClient()->browser()->CreateBrowserMainParts(*params));
  ChromeBrowserMainParts* chrome_main_parts =
      static_cast<ChromeBrowserMainParts*>(browser_main_parts.get());
  EXPECT_TRUE(chrome_main_parts);
  if (chrome_main_parts) {
    base::FieldTrialList field_trial_list_(NULL);
    chrome_main_parts->browser_field_trials_.WarmConnectionFieldTrial();
    EXPECT_EQ(0, net::GetSocketReusePolicy());
  }
}

TEST_F(BrowserMainTest, WarmConnectionFieldTrial_Random) {
  scoped_ptr<content::MainFunctionParams> params(
      new content::MainFunctionParams(command_line_));
  scoped_ptr<content::BrowserMainParts> browser_main_parts(
      content::GetContentClient()->browser()->CreateBrowserMainParts(*params));
  ChromeBrowserMainParts* chrome_main_parts =
      static_cast<ChromeBrowserMainParts*>(browser_main_parts.get());
  EXPECT_TRUE(chrome_main_parts);
  if (chrome_main_parts) {
    const int kNumRuns = 1000;
    for (int i = 0; i < kNumRuns; i++) {
      base::FieldTrialList field_trial_list_(NULL);
      chrome_main_parts->browser_field_trials_.WarmConnectionFieldTrial();
      int val = net::GetSocketReusePolicy();
      EXPECT_LE(val, 2);
      EXPECT_GE(val, 0);
    }
  }
}

#if GTEST_HAS_DEATH_TEST
TEST_F(BrowserMainTest, WarmConnectionFieldTrial_Invalid) {
  command_line_.AppendSwitchASCII(switches::kSocketReusePolicy, "100");

  scoped_ptr<content::MainFunctionParams> params(
      new content::MainFunctionParams(command_line_));
  // This test ends up launching a new process, and that doesn't initialize the
  // ContentClient interfaces.
  scoped_ptr<content::BrowserMainParts> browser_main_parts;
  if (content::GetContentClient()) {
    browser_main_parts.reset(content::GetContentClient()->browser()->
                                 CreateBrowserMainParts(*params));
  } else {
    chrome::ChromeContentBrowserClient client;
    browser_main_parts.reset(client.CreateBrowserMainParts(*params));
  }
  ChromeBrowserMainParts* parts =
      static_cast<ChromeBrowserMainParts*>(browser_main_parts.get());
  EXPECT_TRUE(parts);
  if (parts) {
#if defined(NDEBUG) && defined(DCHECK_ALWAYS_ON)
    EXPECT_DEATH(parts->browser_field_trials_.WarmConnectionFieldTrial(),
                 "Not a valid socket reuse policy group");
#else
    EXPECT_DEBUG_DEATH(parts->browser_field_trials_.WarmConnectionFieldTrial(),
                       "Not a valid socket reuse policy group");
#endif
  }
}
#endif
