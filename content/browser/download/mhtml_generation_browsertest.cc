// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/file_path.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/download/mhtml_generation_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MHTMLGenerationTest : public InProcessBrowserTest {
 public:
  MHTMLGenerationTest() : mhtml_generated_(false), file_size_(0) {}

  void MHTMLGenerated(const FilePath& path, int64 size) {
    mhtml_generated_ = true;
    file_size_ = size;
    MessageLoopForUI::current()->Quit();
  }

 protected:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    InProcessBrowserTest::SetUp();
  }

  bool mhtml_generated() const { return mhtml_generated_; }
  int64 file_size() const { return file_size_; }

  ScopedTempDir temp_dir_;

 private:
  bool mhtml_generated_;
  int64 file_size_;
};

// Tests that generating a MHTML does create contents.
// Note that the actual content of the file is not tested, the purpose of this
// test is to ensure we were successfull in creating the MHTML data from the
// renderer.
IN_PROC_BROWSER_TEST_F(MHTMLGenerationTest, GenerateMHTML) {
  ASSERT_TRUE(test_server()->Start());

  FilePath path(temp_dir_.path());
  path = path.Append(FILE_PATH_LITERAL("test.mht"));

  ui_test_utils::NavigateToURL(browser(),
      test_server()->GetURL("files/google/google.html"));

  TabContents* tab = browser()->GetSelectedTabContents();
  MHTMLGenerationManager* mhtml_generation_manager =
      g_browser_process->mhtml_generation_manager();

  mhtml_generation_manager->GenerateMHTML(tab, path,
      base::Bind(&MHTMLGenerationTest::MHTMLGenerated, this));

  // Block until the MHTML is generated.
  ui_test_utils::RunMessageLoop();

  EXPECT_TRUE(mhtml_generated());
  EXPECT_GT(file_size(), 0);

  // Make sure the actual generated file has some contents.
  int64 file_size;
  ASSERT_TRUE(file_util::GetFileSize(path, &file_size));
  EXPECT_GT(file_size, 100);
}

}  // namespace
