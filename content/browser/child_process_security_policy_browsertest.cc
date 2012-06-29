// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/process_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

class ChildProcessSecurityPolicyInProcessBrowserTest
    : public InProcessBrowserTest {
 public:
  virtual void SetUp() {
    EXPECT_EQ(
      ChildProcessSecurityPolicyImpl::GetInstance()->security_state_.size(),
          0U);
    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() {
    EXPECT_EQ(
      ChildProcessSecurityPolicyImpl::GetInstance()->security_state_.size(),
          0U);
    InProcessBrowserTest::TearDown();
  }
};

IN_PROC_BROWSER_TEST_F(ChildProcessSecurityPolicyInProcessBrowserTest, NoLeak) {
  const FilePath kTestDir(FILE_PATH_LITERAL("google"));
  const FilePath kTestFile(FILE_PATH_LITERAL("google.html"));
  GURL url(ui_test_utils::GetTestUrl(kTestDir, kTestFile));

  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(
      ChildProcessSecurityPolicyImpl::GetInstance()->security_state_.size(),
          1U);

  WebContents* web_contents = chrome::GetWebContentsAt(browser(), 0);
  ASSERT_TRUE(web_contents != NULL);
  base::KillProcess(web_contents->GetRenderProcessHost()->GetHandle(),
                    content::RESULT_CODE_KILLED, true);

  web_contents->GetController().Reload(true);
  EXPECT_EQ(
      1U,
      ChildProcessSecurityPolicyImpl::GetInstance()->security_state_.size());
}
