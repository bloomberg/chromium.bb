// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/layout_browsertest.h"
#include "content/test/content_browser_test_utils.h"
#include "googleurl/src/gurl.h"

// Tests playback and seeking of an audio or video file over file or http based
// on a test parameter.  Test starts with playback, then, after X seconds or the
// ended event fires, seeks near end of file; see player.html for details.  The
// test completes when either the last 'ended' or an 'error' event fires.
class MediaTest
    : public testing::WithParamInterface<bool>,
      public content::ContentBrowserTest {
 public:
  // Play specified audio over http:// or file:// depending on |http| setting.
  void PlayAudio(const char* media_file, bool http) {
    ASSERT_NO_FATAL_FAILURE(PlayMedia("audio", media_file, http));
  }

  // Play specified video over http:// or file:// depending on |http| setting.
  void PlayVideo(const char* media_file, bool http) {
    ASSERT_NO_FATAL_FAILURE(PlayMedia("video", media_file, http));
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // TODO(dalecurtis): Not all Buildbots have viable audio devices, so disable
    // audio to prevent tests from hanging; e.g., a device which is hardware
    // muted.  See http://crbug.com/120749
    command_line->AppendSwitch(switches::kDisableAudio);
  }

 private:
  GURL GetTestURL(const char* tag, const char* media_file, bool http) {
    if (http) {
      return test_server()->GetURL(
          base::StringPrintf("files/media/player.html?%s=%s", tag, media_file));
    }

    FilePath test_file_path = content::GetTestFilePath("media", "player.html");
    std::string query = base::StringPrintf("%s=%s", tag, media_file);
    return content::GetFileUrlWithQuery(test_file_path, query);
  }

  void PlayMedia(const char* tag, const char* media_file, bool http) {
    if (http)
      ASSERT_TRUE(test_server()->Start());

    GURL player_gurl = GetTestURL(tag, media_file, http);

    // Allow the media file to be loaded.
    const string16 kEnded = ASCIIToUTF16("ENDED");
    const string16 kError = ASCIIToUTF16("ERROR");
    const string16 kFailed = ASCIIToUTF16("FAILED");
    content::TitleWatcher title_watcher(shell()->web_contents(), kEnded);
    title_watcher.AlsoWaitForTitle(kFailed);
    title_watcher.AlsoWaitForTitle(kError);

    content::NavigateToURL(shell(), player_gurl);

    string16 final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(kEnded, final_title);
  }
};

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearTheora) {
  PlayVideo("bear.ogv", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearSilentTheora) {
  PlayVideo("bear_silent.ogv", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWebm) {
  PlayVideo("bear.webm", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearSilentWebm) {
  PlayVideo("bear_silent.webm", GetParam());
}

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMp4) {
  PlayVideo("bear.mp4", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearSilentMp4) {
  PlayVideo("bear_silent.mp4", GetParam());
}

// While we support the big endian (be) PCM codecs on Chromium, Quicktime seems
// to be the only creator of this format and only for .mov files.
// TODO(dalecurtis/ihf): Find or create some .wav test cases for "be" format.
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMovPcmS16be) {
  PlayVideo("bear_pcm_s16be.mov", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearMovPcmS24be) {
  PlayVideo("bear_pcm_s24be.mov", GetParam());
}
#endif

#if defined(OS_CHROMEOS)
#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearAviMp3Mpeg4) {
  PlayVideo("bear_mpeg4_mp3.avi", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearAviMp3Divx) {
  PlayVideo("bear_divx_mp3.avi", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBear3gpAacH264) {
  PlayVideo("bear_h264_aac.3gp", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBear3gpAmrnbMpeg4) {
  PlayVideo("bear_mpeg4_amrnb.3gp", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavGsmms) {
  PlayAudio("bear_gsm_ms.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavMulaw) {
  PlayAudio("bear_mulaw.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearFlac) {
  PlayAudio("bear.flac", GetParam());
}
#endif
#endif

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavPcm) {
  PlayAudio("bear_pcm.wav", GetParam());
}

#if defined(OS_MACOSX)
// TODO(dalecurtis): Flaky on Mac 10.6.  http://crbug.com/142896
IN_PROC_BROWSER_TEST_P(MediaTest, FLAKY_VideoTulipWebm) {
  PlayVideo("tulip2.webm", GetParam());
}
#else
IN_PROC_BROWSER_TEST_P(MediaTest, VideoTulipWebm) {
  PlayVideo("tulip2.webm", GetParam());
}
#endif

INSTANTIATE_TEST_CASE_P(File, MediaTest, ::testing::Values(false));
INSTANTIATE_TEST_CASE_P(Http, MediaTest, ::testing::Values(true));

class MediaLayoutTest : public InProcessBrowserLayoutTest {
 protected:
  MediaLayoutTest() : InProcessBrowserLayoutTest(
      FilePath(), FilePath().AppendASCII("media")) {
  }
  virtual ~MediaLayoutTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    InProcessBrowserLayoutTest::SetUpCommandLine(command_line);
    // TODO(dalecurtis): Not all Buildbots have viable audio devices, so disable
    // audio to prevent tests from hanging; e.g., a device which is hardware
    // muted.  See http://crbug.com/120749
    command_line->AppendSwitch(switches::kDisableAudio);
  }
};

// Each browser test can only correspond to a single layout test, otherwise the
// 45 second timeout per test is not long enough for N tests on debug/asan/etc
// builds.

IN_PROC_BROWSER_TEST_F(MediaLayoutTest, VideoAutoplayTest) {
  RunLayoutTest("video-autoplay.html");
}

IN_PROC_BROWSER_TEST_F(MediaLayoutTest, VideoLoopTest) {
  RunLayoutTest("video-loop.html");
}

IN_PROC_BROWSER_TEST_F(MediaLayoutTest, VideoNoAutoplayTest) {
  RunLayoutTest("video-no-autoplay.html");
}
