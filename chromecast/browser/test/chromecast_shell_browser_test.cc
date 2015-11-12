// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/browser/test/chromecast_browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/test_data_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace chromecast {
namespace shell {
namespace {
const char kEnded[] = "ENDED";
const char kError[] = "ERROR";
const char kFailed[] = "FAILED";
}

class ChromecastShellBrowserTest : public ChromecastBrowserTest {
 public:
  ChromecastShellBrowserTest() : url_(url::kAboutBlankURL) {}

  void SetUpOnMainThread() override {
    CreateBrowser();
    NavigateToURL(web_contents(), url_);
  }

  void PlayVideo(const std::string& media_file) {
    PlayMedia("video", media_file);
  }

 private:
  const GURL url_;

  void PlayMedia(const std::string& tag,
                 const std::string& media_file) {
    base::StringPairs query_params;
    query_params.push_back(std::make_pair(tag, media_file));
    RunMediaTestPage("player.html", query_params, kEnded);
  }

  void RunMediaTestPage(const std::string& html_page,
                        const base::StringPairs& query_params,
                        const std::string& expected_title) {
    std::string query = media::GetURLQueryString(query_params);
    GURL gurl = content::GetFileUrlWithQuery(
        media::GetTestDataFilePath(html_page),
        query);

    std::string final_title = RunTest(gurl, expected_title);
    EXPECT_EQ(expected_title, final_title);
  }

  std::string RunTest(const GURL& gurl,
                      const std::string& expected_title) {
    content::TitleWatcher title_watcher(web_contents(),
                                        base::ASCIIToUTF16(expected_title));
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16(kEnded));
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16(kError));
    title_watcher.AlsoWaitForTitle(base::ASCIIToUTF16(kFailed));

    NavigateToURL(web_contents(), gurl);
    base::string16 result = title_watcher.WaitAndGetTitle();
    return base::UTF16ToASCII(result);
  }

  DISALLOW_COPY_AND_ASSIGN(ChromecastShellBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ChromecastShellBrowserTest, EmptyTest) {
  // Run an entire browser lifecycle to ensure nothing breaks.
  // TODO(gunsch): Remove this test case once there are actual assertions to
  // test in a ChromecastBrowserTest instance.
  EXPECT_TRUE(true);
}

IN_PROC_BROWSER_TEST_F(ChromecastShellBrowserTest, MediaPlayback) {
  PlayVideo("bear.mp4");
}

}  // namespace shell
}  // namespace chromecast
