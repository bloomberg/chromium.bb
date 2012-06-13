// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_temp_dir.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/download/save_package.h"

namespace {

const char kTestFile[] = "files/save_page/a.htm";

class SavePackageBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() OVERRIDE {
    ASSERT_TRUE(save_dir_.CreateUniqueTempDir());
    InProcessBrowserTest::SetUp();
  }

  // Returns full paths of destination file and directory.
  void GetDestinationPaths(const std::string& prefix,
                           FilePath* full_file_name,
                           FilePath* dir) {
    *full_file_name = save_dir_.path().AppendASCII(prefix + ".htm");
    *dir = save_dir_.path().AppendASCII(prefix + "_files");
  }

  // Temporary directory we will save pages to.
  ScopedTempDir save_dir_;
};

// Create a SavePackage and delete it without calling Init.
// SavePackage dtor has various asserts/checks that should not fire.
IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, ImplicitCancel) {
  ASSERT_TRUE(test_server()->Start());
  GURL url = test_server()->GetURL(kTestFile);
  ui_test_utils::NavigateToURL(browser(), url);
  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  scoped_refptr<SavePackage> save_package(new SavePackage(
      browser()->GetActiveWebContents(),
      content::SAVE_PAGE_TYPE_AS_ONLY_HTML, full_file_name, dir));
  ASSERT_TRUE(test_server()->Stop());
}

// Create a SavePackage, call Cancel, then delete it.
// SavePackage dtor has various asserts/checks that should not fire.
IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, ExplicitCancel) {
  ASSERT_TRUE(test_server()->Start());
  GURL url = test_server()->GetURL(kTestFile);
  ui_test_utils::NavigateToURL(browser(), url);
  FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  scoped_refptr<SavePackage> save_package(new SavePackage(
      browser()->GetActiveWebContents(),
      content::SAVE_PAGE_TYPE_AS_ONLY_HTML, full_file_name, dir));
  save_package->Cancel(true);
  ASSERT_TRUE(test_server()->Stop());
}

}  // namespace
