// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app.h"

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#endif

using content::RenderViewHostTester;

class WebApplicationTest : public ChromeRenderViewHostTestHarness {
 protected:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
#if defined(TOOLKIT_VIEWS)
    extensions::TabHelper::CreateForWebContents(web_contents());
    FaviconTabHelper::CreateForWebContents(web_contents());
#endif
  }
};

#if defined(TOOLKIT_VIEWS)
TEST_F(WebApplicationTest, GetShortcutInfoForTab) {
  const base::string16 title = base::ASCIIToUTF16("TEST_TITLE");
  const base::string16 description = base::ASCIIToUTF16("TEST_DESCRIPTION");
  const GURL url("http://www.foo.com/bar");
  WebApplicationInfo web_app_info;
  web_app_info.title = title;
  web_app_info.description = description;
  web_app_info.app_url = url;

  RenderViewHostTester::TestOnMessageReceived(
      rvh(), ChromeViewHostMsg_DidGetWebApplicationInfo(0, web_app_info));
  web_app::ShortcutInfo info;
  web_app::GetShortcutInfoForTab(web_contents(), &info);

  EXPECT_EQ(title, info.title);
  EXPECT_EQ(description, info.description);
  EXPECT_EQ(url, info.url);
}
#endif

#if defined(ENABLE_EXTENSIONS)
TEST_F(WebApplicationTest, AppDirWithId) {
  base::FilePath profile_path(FILE_PATH_LITERAL("profile"));
  base::FilePath result(
      web_app::GetWebAppDataDirectory(profile_path, "123", GURL()));
  base::FilePath expected = profile_path.AppendASCII("Web Applications")
                                  .AppendASCII("_crx_123");
  EXPECT_EQ(expected, result);
}

TEST_F(WebApplicationTest, AppDirWithUrl) {
  base::FilePath profile_path(FILE_PATH_LITERAL("profile"));
  base::FilePath result(web_app::GetWebAppDataDirectory(
      profile_path, std::string(), GURL("http://example.com")));
  base::FilePath expected = profile_path.AppendASCII("Web Applications")
      .AppendASCII("example.com").AppendASCII("http_80");
  EXPECT_EQ(expected, result);
}
#endif // ENABLE_EXTENSIONS
