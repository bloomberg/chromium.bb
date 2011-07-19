// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

class WebApplicationTest : public TabContentsWrapperTestHarness {
 public:
  WebApplicationTest()
      : TabContentsWrapperTestHarness(),
        ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 private:
  // Supply our own profile so we use the correct profile data. The test harness
  // is not supposed to overwrite a profile if it's already created.
  virtual void SetUp() {
    profile_.reset(new TestingProfile());

    TabContentsWrapperTestHarness::SetUp();
  }

  virtual void TearDown() {
    TabContentsWrapperTestHarness::TearDown();

    profile_.reset(NULL);
  }

  BrowserThread ui_thread_;
};

TEST_F(WebApplicationTest, GetShortcutInfoForTab) {
  const string16 title = ASCIIToUTF16("TEST_TITLE");
  const string16 description = ASCIIToUTF16("TEST_DESCRIPTION");
  const GURL url("http://www.foo.com/bar");
  WebApplicationInfo web_app_info;
  web_app_info.title = title;
  web_app_info.description = description;
  web_app_info.app_url = url;

  rvh()->TestOnMessageReceived(
      ExtensionHostMsg_DidGetApplicationInfo(0, 0, web_app_info));
  ShellIntegration::ShortcutInfo info;
  web_app::GetShortcutInfoForTab(contents_wrapper(), &info);

  EXPECT_EQ(title, info.title);
  EXPECT_EQ(description, info.description);
  EXPECT_EQ(url, info.url);
}

TEST_F(WebApplicationTest, GetDataDir) {
  FilePath test_path(FILE_PATH_LITERAL("/path/to/test"));
  FilePath result = web_app::GetDataDir(test_path);
  test_path = test_path.AppendASCII("Web Applications");
  EXPECT_EQ(test_path.value(), result.value());
}
