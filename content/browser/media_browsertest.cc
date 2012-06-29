// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/test/layout_browsertest.h"
#include "googleurl/src/gurl.h"

class MediaTest : public InProcessBrowserTest {
 protected:
  GURL GetTestURL(const char* tag, const char* media_file) {
    FilePath test_file_path = ui_test_utils::GetTestFilePath(
        FilePath(FILE_PATH_LITERAL("media")),
        FilePath(FILE_PATH_LITERAL("player.html")));
    std::string query = base::StringPrintf("%s=%s", tag, media_file);
    return ui_test_utils::GetFileUrlWithQuery(test_file_path, query);
  }

  void PlayMedia(const char* tag, const char* media_file) {
    GURL player_gurl = GetTestURL(tag, media_file);

    // Allow the media file to be loaded.
    const string16 kPlaying = ASCIIToUTF16("PLAYING");
    const string16 kFailed = ASCIIToUTF16("FAILED");
    const string16 kError = ASCIIToUTF16("ERROR");
    ui_test_utils::TitleWatcher title_watcher(
        chrome::GetActiveWebContents(browser()), kPlaying);
    title_watcher.AlsoWaitForTitle(kFailed);
    title_watcher.AlsoWaitForTitle(kError);

    ui_test_utils::NavigateToURL(browser(), player_gurl);

    string16 final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(kPlaying, final_title);
  }

  void PlayAudio(const char* url) {
    PlayMedia("audio", url);
  }

  void PlayVideo(const char* url) {
    PlayMedia("video", url);
  }
};

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearTheora) {
  PlayVideo("bear.ogv");
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearSilentTheora) {
  PlayVideo("bear_silent.ogv");
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearWebm) {
  PlayVideo("bear.webm");
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearSilentWebm) {
  PlayVideo("bear_silent.webm");
}

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearMp4) {
  PlayVideo("bear.mp4");
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearSilentMp4) {
  PlayVideo("bear_silent.mp4");
}
#endif

#if defined(OS_CHROMEOS)
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearAviMp3Mpeg4) {
  PlayVideo("bear_mpeg4_mp3.avi");
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearAviMp3Divx) {
  PlayVideo("bear_divx_mp3.avi");
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBear3gpAacH264) {
  PlayVideo("bear_h264_aac.3gp");
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBear3gpAmrnbMpeg4) {
  PlayVideo("bear_mpeg4_amrnb.3gp");
}

// TODO(ihf): Enable these audio codecs for CrOS.
// IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearWavAlaw) {
//   PlayVideo("bear_alaw.wav");
// }
// IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearWavGsmms) {
//   PlayVideo("bear_gsmms.wav");
// }

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearWavMulaw) {
  PlayAudio("bear_mulaw.wav");
}

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearFlac) {
  PlayAudio("bear.flac");
}
#endif
#endif

IN_PROC_BROWSER_TEST_F(MediaTest, VideoBearWavPcm) {
  PlayAudio("bear_pcm.wav");
}

class MediaLayoutTest : public InProcessBrowserLayoutTest {
 protected:
  MediaLayoutTest() : InProcessBrowserLayoutTest(
      FilePath(), FilePath().AppendASCII("media")) {
  }
  virtual ~MediaLayoutTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    InProcessBrowserLayoutTest::SetUpInProcessBrowserTestFixture();
    AddResourceForLayoutTest(FilePath().AppendASCII("media"),
                             FilePath().AppendASCII("content"));
    AddResourceForLayoutTest(FilePath().AppendASCII("media"),
                             FilePath().AppendASCII("media-file.js"));
    AddResourceForLayoutTest(FilePath().AppendASCII("media"),
                             FilePath().AppendASCII("media-fullscreen.js"));
    AddResourceForLayoutTest(FilePath().AppendASCII("media"),
                             FilePath().AppendASCII("video-paint-test.js"));
    AddResourceForLayoutTest(FilePath().AppendASCII("media"),
                             FilePath().AppendASCII("video-played.js"));
    AddResourceForLayoutTest(FilePath().AppendASCII("media"),
                             FilePath().AppendASCII("video-test.js"));
  }
};

// Each browser test can only correspond to a single layout test, otherwise the
// 45 second timeout per test is not long enough for N tests on debug/asan/etc
// builds.

IN_PROC_BROWSER_TEST_F(MediaLayoutTest, VideoAutoplayTest) {
  RunLayoutTest("video-autoplay.html");
}

// TODO(dalecurtis): Disabled because loop is flaky.  http://crbug.com/134021
// IN_PROC_BROWSER_TEST_F(MediaLayoutTest, VideoLoopTest) {
//   RunLayoutTest("video-loop.html");
// }

IN_PROC_BROWSER_TEST_F(MediaLayoutTest, VideoNoAutoplayTest) {
  RunLayoutTest("video-no-autoplay.html");
}
