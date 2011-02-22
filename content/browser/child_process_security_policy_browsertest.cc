// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/process_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/result_codes.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChildProcessSecurityPolicyInProcessBrowserTest
    : public InProcessBrowserTest {
 public:
  virtual void SetUp() {
    EXPECT_EQ(
      ChildProcessSecurityPolicy::GetInstance()->security_state_.size(), 0U);
    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() {
    EXPECT_EQ(
      ChildProcessSecurityPolicy::GetInstance()->security_state_.size(), 0U);
    InProcessBrowserTest::TearDown();
  }
};

IN_PROC_BROWSER_TEST_F(ChildProcessSecurityPolicyInProcessBrowserTest, NoLeak) {
  const FilePath kTestDir(FILE_PATH_LITERAL("google"));
  const FilePath kTestFile(FILE_PATH_LITERAL("google.html"));
  GURL url(ui_test_utils::GetTestUrl(kTestDir, kTestFile));

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(
      ChildProcessSecurityPolicy::GetInstance()->security_state_.size(), 1U);

  TabContents* tab = browser()->GetTabContentsAt(0);
  ASSERT_TRUE(tab != NULL);
  base::KillProcess(tab->GetRenderProcessHost()->GetHandle(),
                    ResultCodes::KILLED, true);

  tab->controller().Reload(true);
  EXPECT_EQ(
      ChildProcessSecurityPolicy::GetInstance()->security_state_.size(), 1U);
}
