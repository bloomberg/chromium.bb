// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test_utils.h"
#include "content/test/layout_browsertest.h"
#include "googleurl/src/gurl.h"

// Common media types.
static const char kWebMAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
static const char kWebMVideoOnly[] = "video/webm; codecs=\"vp8\"";
static const char kWebMAudioVideo[] = "video/webm; codecs=\"vorbis, vp8\"";

// Common test expectations.
const string16 kEnded = ASCIIToUTF16("ENDED");
const string16 kError = ASCIIToUTF16("ERROR");
const string16 kFailed = ASCIIToUTF16("FAILED");

namespace content {

class MediaSourceTest :  public ContentBrowserTest {
 public:
  void TestSimplePlayback(const char* media_file, const char* media_type,
                          const string16 expectation) {
    ASSERT_NO_FATAL_FAILURE(
        RunTest("media_source_player.html", media_file, media_type,
                expectation));
  }

  void RunTest(const char* html_page, const char* media_file,
               const char* media_type, const string16 expectation) {
    ASSERT_TRUE(test_server()->Start());
    GURL player_gurl = test_server()->GetURL(base::StringPrintf(
        "files/media/%s?mediafile=%s&mediatype=%s", html_page, media_file,
        media_type));
    TitleWatcher title_watcher(shell()->web_contents(), expectation);
    title_watcher.AlsoWaitForTitle(kError);
    title_watcher.AlsoWaitForTitle(kFailed);

    NavigateToURL(shell(), player_gurl);
    string16 final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expectation, final_title);

    if (final_title == kFailed) {
      std::string fail_message;
      EXPECT_TRUE(ExecuteScriptAndExtractString(
          shell()->web_contents(),
          "window.domAutomationController.send(failMessage);",
          &fail_message));
      LOG(INFO) << "Test failed: " << fail_message;
    }
  }
};

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240.webm", kWebMAudioVideo, kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-video-only.webm", kWebMVideoOnly, kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-320x240-audio-only.webm", kWebMAudioOnly, kEnded);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, Playback_Type_Error) {
  TestSimplePlayback("bear-320x240-video-only.webm", kWebMAudioOnly, kError);
}

IN_PROC_BROWSER_TEST_F(MediaSourceTest, ConfigChangeVideo) {
  ASSERT_NO_FATAL_FAILURE(
      RunTest("mse_config_change.html", NULL, NULL, kEnded));
}

}  // namespace content