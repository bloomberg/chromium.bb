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

// TODO(wolenetz): Fix Media.YUV* tests on MSVS 2012 x64. crbug.com/180074
#if defined(OS_WIN) && defined(ARCH_CPU_X86_64) && _MSC_VER == 1700
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

namespace content {

// Tests playback and seeking of an audio or video file over file or http based
// on a test parameter.  Test starts with playback, then, after X seconds or the
// ended event fires, seeks near end of file; see player.html for details.  The
// test completes when either the last 'ended' or an 'error' event fires.
class MediaTest : public testing::WithParamInterface<bool>,
                  public ContentBrowserTest {
 public:
  // Play specified audio over http:// or file:// depending on |http| setting.
  void PlayAudio(const char* media_file, bool http) {
    PlayMedia("audio", media_file, http);
  }

  // Play specified video over http:// or file:// depending on |http| setting.
  void PlayVideo(const char* media_file, bool http) {
    PlayMedia("video", media_file, http);
  }

  // Run specified color format test with the expected result.
  void RunColorFormatTest(const char* media_file, const char* expected) {
    base::FilePath test_file_path = GetTestFilePath("media", "blackwhite.html");
    RunTest(GetFileUrlWithQuery(test_file_path, media_file), expected);
  }

 private:
  void PlayMedia(const char* tag, const char* media_file, bool http) {
    GURL gurl;

    if (http) {
      if (!test_server()->Start()) {
        ADD_FAILURE() << "Failed to start test server";
        return;
      }

      gurl = test_server()->GetURL(
          base::StringPrintf("files/media/player.html?%s=%s", tag, media_file));
    } else {
      base::FilePath test_file_path = GetTestFilePath("media", "player.html");
      gurl = GetFileUrlWithQuery(
          test_file_path, base::StringPrintf("%s=%s", tag, media_file));
    }

    RunTest(gurl, "ENDED");
  }

  void RunTest(const GURL& gurl, const char* expected) {
    const string16 kExpected = ASCIIToUTF16(expected);
    const string16 kEnded = ASCIIToUTF16("ENDED");
    const string16 kError = ASCIIToUTF16("ERROR");
    const string16 kFailed = ASCIIToUTF16("FAILED");
    DCHECK(kExpected == kEnded || kExpected == kError || kExpected == kFailed)
        << kExpected;

    TitleWatcher title_watcher(shell()->web_contents(), kEnded);
    title_watcher.AlsoWaitForTitle(kFailed);
    title_watcher.AlsoWaitForTitle(kError);

    NavigateToURL(shell(), gurl);

    string16 final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(kExpected, final_title);
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

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearAviMp3Mpeg4Asp) {
  PlayVideo("bear_mpeg4asp_mp3.avi", GetParam());
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

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavPcm3kHz) {
  PlayAudio("bear_3kHz.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoBearWavPcm192kHz) {
  PlayAudio("bear_192kHz.wav", GetParam());
}

IN_PROC_BROWSER_TEST_P(MediaTest, VideoTulipWebm) {
  PlayVideo("tulip2.webm", GetParam());
}

INSTANTIATE_TEST_CASE_P(File, MediaTest, ::testing::Values(false));
INSTANTIATE_TEST_CASE_P(Http, MediaTest, ::testing::Values(true));

class MediaLayoutTest : public InProcessBrowserLayoutTest {
 protected:
  MediaLayoutTest() : InProcessBrowserLayoutTest(
      base::FilePath(), base::FilePath().AppendASCII("media")) {
  }
  virtual ~MediaLayoutTest() {}
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

IN_PROC_BROWSER_TEST_F(MediaTest, MAYBE(Yuv420pTheora)) {
  RunColorFormatTest("yuv420p.ogv", "ENDED");
}

IN_PROC_BROWSER_TEST_F(MediaTest, MAYBE(Yuv422pTheora)) {
  RunColorFormatTest("yuv422p.ogv", "ENDED");
}

IN_PROC_BROWSER_TEST_F(MediaTest, MAYBE(Yuv444pTheora)) {
  // TODO(scherkus): Support YUV444 http://crbug.com/104711
  RunColorFormatTest("yuv424p.ogv", "ERROR");
}

IN_PROC_BROWSER_TEST_F(MediaTest, MAYBE(Yuv420pVp8)) {
  RunColorFormatTest("yuv420p.webm", "ENDED");
}

#if defined(GOOGLE_CHROME_BUILD) || defined(USE_PROPRIETARY_CODECS)
IN_PROC_BROWSER_TEST_F(MediaTest, MAYBE(Yuv420pH264)) {
  RunColorFormatTest("yuv420p.mp4", "ENDED");
}

IN_PROC_BROWSER_TEST_F(MediaTest, MAYBE(Yuvj420pH264)) {
  RunColorFormatTest("yuvj420p.mp4", "ENDED");
}

IN_PROC_BROWSER_TEST_F(MediaTest, MAYBE(Yuv422pH264)) {
  RunColorFormatTest("yuv422p.mp4", "ENDED");
}

IN_PROC_BROWSER_TEST_F(MediaTest, MAYBE(Yuv444pH264)) {
  // TODO(scherkus): Support YUV444 http://crbug.com/104711
  RunColorFormatTest("yuv444p.mp4", "ERROR");
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(MediaTest, Yuv420pMpeg4) {
  RunColorFormatTest("yuv420p.avi", "ENDED");
}
#endif
#endif

}  // namespace content
