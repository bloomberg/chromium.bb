// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::RenderViewHostTester;

class WebApplicationTest : public TabContentsTestHarness {
 public:
  WebApplicationTest() : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 private:
  content::TestBrowserThread ui_thread_;
};

#if defined(OS_MACOSX)
#define MAYBE_GetShortcutInfoForTab DISABLED_GetShortcutInfoForTab
#else
#define MAYBE_GetShortcutInfoForTab GetShortcutInfoForTab
#endif
TEST_F(WebApplicationTest, MAYBE_GetShortcutInfoForTab) {
  const string16 title = ASCIIToUTF16("TEST_TITLE");
  const string16 description = ASCIIToUTF16("TEST_DESCRIPTION");
  const GURL url("http://www.foo.com/bar");
  WebApplicationInfo web_app_info;
  web_app_info.title = title;
  web_app_info.description = description;
  web_app_info.app_url = url;

  RenderViewHostTester::TestOnMessageReceived(
      rvh(),
      ExtensionHostMsg_DidGetApplicationInfo(0, 0, web_app_info));
  ShellIntegration::ShortcutInfo info;
  web_app::GetShortcutInfoForTab(tab_contents(), &info);

  EXPECT_EQ(title, info.title);
  EXPECT_EQ(description, info.description);
  EXPECT_EQ(url, info.url);
}

TEST_F(WebApplicationTest, AppDirWithId) {
  FilePath profile_path(FILE_PATH_LITERAL("profile"));
  FilePath result(web_app::GetWebAppDataDirectory(profile_path, "123", GURL()));
  FilePath expected = profile_path.AppendASCII("Web Applications")
                                  .AppendASCII("_crx_123");
  EXPECT_EQ(expected, result);
}

TEST_F(WebApplicationTest, AppDirWithUrl) {
  FilePath profile_path(FILE_PATH_LITERAL("profile"));
  FilePath result(web_app::GetWebAppDataDirectory(
      profile_path, "", GURL("http://example.com")));
  FilePath expected = profile_path.AppendASCII("Web Applications")
                                  .AppendASCII("example.com")
                                  .AppendASCII("http_80");
  EXPECT_EQ(expected, result);
}
