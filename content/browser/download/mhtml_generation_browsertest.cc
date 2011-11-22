// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/download/mhtml_generation_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_types.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MHTMLGenerationTest : public InProcessBrowserTest {
 public:
  MHTMLGenerationTest() {}

 protected:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    InProcessBrowserTest::SetUp();
  }

  ScopedTempDir temp_dir_;
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

  content::Source<RenderViewHost> source(tab->render_view_host());
  ui_test_utils::WindowedNotificationObserverWithDetails<
      MHTMLGenerationManager::NotificationDetails> signal(
          content::NOTIFICATION_MHTML_GENERATED, source);
  mhtml_generation_manager->GenerateMHTML(tab, path);
  signal.Wait();

  MHTMLGenerationManager::NotificationDetails details;
  ASSERT_TRUE(signal.GetDetailsFor(source.map_key(), &details));
  ASSERT_GT(details.file_size, 0);

  // Make sure the actual generated file has some contents.
  int64 file_size;
  ASSERT_TRUE(file_util::GetFileSize(path, &file_size));
  EXPECT_GT(file_size, 100);
}

}  // namespace
