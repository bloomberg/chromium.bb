// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "content/browser/media/media_browsertest.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

const char kProbably[] = "probably";
const char kMaybe[] = "maybe";
const char kNot[] = "";

#if defined(USE_PROPRIETARY_CODECS)
const char kPropProbably[] = "probably";
const char kPropMaybe[] = "maybe";
#else
const char kPropProbably[] = "";
const char kPropMaybe[] = "";
#endif  // USE_PROPRIETARY_CODECS

#if !defined(OS_ANDROID)
const char kOggVideoProbably[] = "probably";
const char kOggVideoMaybe[] = "maybe";
const char kTheoraProbably[] = "probably";
const char kOggOpusProbably[] = "probably";
const char* kMpeg2AacProbably = kPropProbably;
#else
const char kOggVideoProbably[] = "";
const char kOggVideoMaybe[] = "";
const char kTheoraProbably[] = "";
const char kOggOpusProbably[] = "";
const char kMpeg2AacProbably[] = "";
#endif  // !OS_ANDROID

namespace content {

class MediaCanPlayTypeTest : public MediaBrowserTest {
 public:
  MediaCanPlayTypeTest() : url_("about:blank") { }

  void SetUpOnMainThread() override { NavigateToURL(shell(), url_); }

  std::string CanPlay(const std::string& type) {
    std::string command("document.createElement('video').canPlayType(");
    command.append(type);
    command.append(")");

    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        shell()->web_contents(),
        "window.domAutomationController.send(" + command + ");",
        &result));
    return result;
  }

  void TestMPEGUnacceptableCombinations(const std::string& mime) {
    // AVC codecs must be followed by valid 6-digit hexadecimal number.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.12345\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.12345\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.1234567\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.1234567\"'"));
    // TODO(ddorwin): These four should return "". See crbug.com/457076.
//    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.number\"'"));
//    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.number\"'"));
//    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.12345.\"'"));
//    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.12345.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.123456.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.123456.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.123456.7\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.123456.7\"'"));

    // AAC codecs must be followed by one or two valid hexadecimal numbers.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.no\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.0k\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.0k.0k\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.4.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.0k\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40k\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.2k\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.2k\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.2.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.2.0\"'"));

    // Unlike just "avc1", just "mp4a" is not supported.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.\"'"));

    // Other names for the codecs are not supported.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"h264\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"h.264\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"H264\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"H.264\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"aac\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AAC\"'"));

    // Codecs must not end with a dot.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.\"'"));

    // A simple substring match is not sufficient.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"lavc1337\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\";mp4a+\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\";mp4a.40+\"'"));

    // Codecs not belonging to MPEG container.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.4D401E, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.64001F, vorbis\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1, opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3, opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.4D401E, opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.64001F, opus\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8, mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9, mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8, mp4a.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9, mp4a.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp8, mp4a.40.02\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"vp9, mp4a.40.02\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.4D401E, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.64001F, 1\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora, mp4a\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora, mp4a.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora, mp4a.40.02\"'"));

    // Codecs are case sensitive.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC1.4d401e\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC3.64001f\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"MP4A\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"MP4A.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"MP4A.40.02\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC1, MP4\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC3, MP4\"'"));
    EXPECT_EQ(kNot,
              CanPlay("'" + mime + "; codecs=\", AVC1.4D401E, MP4.40.2\"'"));
    EXPECT_EQ(kNot,
              CanPlay("'" + mime + "; codecs=\", AVC3.64001F, MP4.40.2\"'"));
    EXPECT_EQ(kNot,
              CanPlay("'" + mime + "; codecs=\", AVC1.4D401E, MP4.40.02\"'"));
    EXPECT_EQ(kNot,
              CanPlay("'" + mime + "; codecs=\", AVC3.64001F, MP4.40.02\"'"));

    // Unknown codecs.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc4\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1x\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3x\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4ax\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"unknown\"'"));
  }

  void TestOGGUnacceptableCombinations(const std::string& mime) {
    // Codecs not belonging to OGG container.
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp8\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp8.0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp8, opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp8, vorbis\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp9\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp9.0\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp9, opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp9, vorbis\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc1.4D401E\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc3.64001F\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc1, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc3, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc1, opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc3, opus\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"mp4a.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"mp4a.40.02\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"theora, mp4a.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"theora, mp4a.40.02\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"theora, 1\"'"));

    // Codecs are case sensitive.
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"Theora\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"Opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"Vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"Theora, Opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"Theora, Vorbis\"'"));

    // Unknown codecs.
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"unknown\"'"));
  }

  void TestWEBMUnacceptableCombinations(const std::string& mime) {
    // Codecs not belonging to WEBM container.
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp8, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp9, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp8.0, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp9.0, 1\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"theora\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"theora, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"theora, opus\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc1.4D401E\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc3.64001F\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc1, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc3, vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc1, opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc3, opus\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"mp4a.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"mp4a.40.02\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp8, mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp9, mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp8.0, mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp9.0, mp4a.40\"'"));

    // Codecs are case sensitive.
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"VP8, Vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"VP8.0, Opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"VP9, Vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"VP9.0, Opus\"'"));

    // Unknown codec.
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"unknown\"'"));
  }

  void TestWAVUnacceptableCombinations(const std::string& mime) {
    // Codecs not belonging to WAV container.
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp8\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp9\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp8.0, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vp9.0, 1\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"vorbis\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"opus\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"theora\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"theora, 1\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc1.4D401E\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc3.64001F\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc1, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"avc3, 1\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"mp4a.40\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"mp4a.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"mp4a.40.02\"'"));

    // Unknown codec.
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"unknown\"'"));
  }

 private:
  GURL url_;
  DISALLOW_COPY_AND_ASSIGN(MediaCanPlayTypeTest);
};

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_wav) {
  EXPECT_EQ(kMaybe, CanPlay("'audio/wav'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/wav; codecs=\"1\"'"));

  TestWAVUnacceptableCombinations("audio/wav");

  EXPECT_EQ(kMaybe, CanPlay("'audio/x-wav'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/x-wav; codecs=\"1\"'"));

  TestWAVUnacceptableCombinations("audio/x-wav");
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_webm) {
  // On Android, VP9 is supported only on KitKat and above (API level 19) and
  // Opus is supported only on Lollipop and above (API level 21).
  std::string VP9Probably = "probably";
  std::string VP9AndOpusProbably = "probably";
  std::string OpusProbably = "probably";
#if defined(OS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 19)
    VP9Probably = "";
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 21) {
    OpusProbably = "";
    VP9AndOpusProbably = "";
  }
#endif
  EXPECT_EQ(kMaybe, CanPlay("'video/webm'"));

  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8, vorbis\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8.0, vorbis\"'"));
  EXPECT_EQ(OpusProbably, CanPlay("'video/webm; codecs=\"vp8, opus\"'"));
  EXPECT_EQ(OpusProbably, CanPlay("'video/webm; codecs=\"vp8.0, opus\"'"));

  EXPECT_EQ(VP9Probably, CanPlay("'video/webm; codecs=\"vp9\"'"));
  EXPECT_EQ(VP9Probably, CanPlay("'video/webm; codecs=\"vp9.0\"'"));
  EXPECT_EQ(VP9Probably, CanPlay("'video/webm; codecs=\"vp9, vorbis\"'"));
  EXPECT_EQ(VP9Probably, CanPlay("'video/webm; codecs=\"vp9.0, vorbis\"'"));
  EXPECT_EQ(VP9AndOpusProbably, CanPlay("'video/webm; codecs=\"vp9, opus\"'"));
  EXPECT_EQ(VP9AndOpusProbably,
            CanPlay("'video/webm; codecs=\"vp9.0, opus\"'"));

  EXPECT_EQ(VP9Probably, CanPlay("'video/webm; codecs=\"vp8, vp9\"'"));
  EXPECT_EQ(VP9Probably, CanPlay("'video/webm; codecs=\"vp8.0, vp9.0\"'"));

  TestWEBMUnacceptableCombinations("video/webm");

  EXPECT_EQ(kMaybe, CanPlay("'audio/webm'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/webm; codecs=\"vorbis\"'"));
  EXPECT_EQ(OpusProbably, CanPlay("'audio/webm; codecs=\"opus\"'"));
  EXPECT_EQ(OpusProbably, CanPlay("'audio/webm; codecs=\"opus, vorbis\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8.0, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8, opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp8.0, opus\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9.0\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9.0, vorbis\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9, opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/webm; codecs=\"vp9.0, opus\"'"));

  TestWEBMUnacceptableCombinations("audio/webm");
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_ogg) {
  EXPECT_EQ(kOggVideoMaybe, CanPlay("'video/ogg'"));
  EXPECT_EQ(kOggVideoProbably, CanPlay("'video/ogg; codecs=\"theora\"'"));
  EXPECT_EQ(kOggVideoProbably,
            CanPlay("'video/ogg; codecs=\"theora, vorbis\"'"));
  EXPECT_EQ(kOggVideoProbably,
            CanPlay("'video/ogg; codecs=\"theora, opus\"'"));
  EXPECT_EQ(kOggVideoProbably,
            CanPlay("'video/ogg; codecs=\"opus, vorbis\"'"));

  TestOGGUnacceptableCombinations("video/ogg");

  EXPECT_EQ(kMaybe, CanPlay("'audio/ogg'"));
  EXPECT_EQ(kProbably, CanPlay("'audio/ogg; codecs=\"vorbis\"'"));
  EXPECT_EQ(kOggOpusProbably, CanPlay("'audio/ogg; codecs=\"opus\"'"));
  EXPECT_EQ(kOggOpusProbably, CanPlay("'audio/ogg; codecs=\"vorbis, opus\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"theora, opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"theora, vorbis\"'"));

  TestOGGUnacceptableCombinations("audio/ogg");

  EXPECT_EQ(kMaybe, CanPlay("'application/ogg'"));
  EXPECT_EQ(kProbably, CanPlay("'application/ogg; codecs=\"vorbis\"'"));
  EXPECT_EQ(kTheoraProbably, CanPlay("'application/ogg; codecs=\"theora\"'"));
  EXPECT_EQ(kOggOpusProbably, CanPlay("'application/ogg; codecs=\"opus\"'"));
  EXPECT_EQ(kTheoraProbably,
            CanPlay("'application/ogg; codecs=\"theora, vorbis\"'"));
  EXPECT_EQ(kTheoraProbably,
            CanPlay("'application/ogg; codecs=\"theora, opus\"'"));
  EXPECT_EQ(kOggOpusProbably,
            CanPlay("'application/ogg; codecs=\"opus, vorbis\"'"));

  TestOGGUnacceptableCombinations("application/ogg");
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_mp3) {
  EXPECT_EQ(kNot, CanPlay("'video/mp3'"));
  EXPECT_EQ(kNot, CanPlay("'video/mpeg'"));
  EXPECT_EQ(kNot, CanPlay("'video/x-mp3'"));

  // audio/mpeg without a codecs parameter (RFC 3003 compliant)
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mpeg'"));

  // audio/mpeg with mp3 in codecs parameter. (Not RFC compliant, but
  // very common in the wild so it is a defacto standard).
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mpeg; codecs=\"mp3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"mp4a.40.02\"'"));

  TestMPEGUnacceptableCombinations("audio/mpeg");

  // audio/mp3 does not allow any codecs parameter
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp3'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a.40.02\"'"));

  TestMPEGUnacceptableCombinations("audio/mp3");

  // audio/x-mp3 does not allow any codecs parameter
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-mp3'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4a.40.02\"'"));

  TestMPEGUnacceptableCombinations("audio/x-mp3");
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_mp4) {
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1, mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3, mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1, avc3\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42801E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42C01E\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.42E11E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.42101E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.42701E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42F01E\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"mp4a.40.02\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc1.42E01E, mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc1.42E01E, mp4a.40.02\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc3.42E01E, mp4a.40.5\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc3.42E01E, mp4a.40.05\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc3.42E01E, mp4a.40.29\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1, mp4a.40.2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1, mp4a.40.02\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3, mp4a.40.2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3, mp4a.40.02\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'video/mp4; codecs=\"avc1.42E01E, mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'video/mp4; codecs=\"avc3.42E01E, mp4a.40\"'"));

  TestMPEGUnacceptableCombinations("video/mp4");

  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc3\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1, mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc3, mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1, avc3\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc1.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc3.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc3.42801E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc3.42C01E\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1.42E11E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1.42101E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1.42701E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"avc1.42F01E\"'"));

  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"mp4a.40.02\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc1.42E01E, mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc1.42E01E, mp4a.40.02\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc3.42E01E, mp4a.40.5\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc3.42E01E, mp4a.40.05\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc3.42E01E, mp4a.40.29\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1, mp4a.40.2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1, mp4a.40.02\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc3, mp4a.40.2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc3, mp4a.40.02\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'video/x-m4v; codecs=\"avc1.42E01E, mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'video/x-m4v; codecs=\"avc3.42E01E, mp4a.40\"'"));

  TestMPEGUnacceptableCombinations("video/x-m4v");

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"mp4a.40\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.02\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.5\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.05\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.29\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"avc3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"avc1, mp4a.40\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"avc3, mp4a.40\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"avc3.64001F\"'"));

  TestMPEGUnacceptableCombinations("audio/mp4");

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/x-m4a; codecs=\"mp4a.40\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.40.02\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.40.5\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.40.05\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-m4a; codecs=\"mp4a.40.29\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc1, mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc3, mp4a\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc3.64001F\"'"));

  TestMPEGUnacceptableCombinations("audio/x-m4a");
}

// When modifying this test, also change CodecSupportTest_Avc3Variants.
IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_Avc1Variants) {
  // avc1 without extensions results in "maybe" for compatibility.
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1\"'"));

  // Any 6-digit hexadecimal number will result in at least "maybe".
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.123456\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.ABCDEF\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.abcdef\"'"));
  EXPECT_EQ(kNot,       CanPlay("'video/mp4; codecs=\"avc1.12345\"'"));
  EXPECT_EQ(kNot,       CanPlay("'video/mp4; codecs=\"avc1.1234567\"'"));

  // Both upper and lower case hexadecimal digits are accepted.
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42e01e\"'"));

  // From a YouTube DASH MSE test manifest.
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.4d401f\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.4d401e\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.4d4015\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.640028\"'"));

  //
  // Baseline Profile (66 == 0x42).
  //  The first four digits must be 42E0, but Chrome also allows 42[8-F]0.
  //  (See http://crbug.com/408552#c17.)
  //  The last two digits must be any valid level.
  //
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E00A\"'"));

  // The third digit must be 8-F.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42001E\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42101E\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42201E\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42301E\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42401E\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42501E\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42601E\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42701E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42801E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42901E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42A01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42B01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42C01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42D01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42F01E\"'"));

  // The fourth digit must be 0.
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.42E11E\"'"));

  //
  // Main Profile (77 == 0x4D).
  //  The first four digits must be 4D40.
  //  The last two digits must be any valid level.
  //
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.4D400A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.4D401E\"'"));

  // Other values are not allowed for the third and fourth digits.
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.4D301E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.4D501E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.4D411E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.4D4F1E\"'"));

  //
  // High Profile (100 == 0x64).
  //  The first four digits must be 6400.
  //  The last two digits must be any valid level.
  //
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.64000A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.64001E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.64001F\"'"));

  // Other values are not allowed for the third and fourth digits.
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.64101E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.64f01E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.64011E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.640F1E\"'"));

  //
  //  Other profiles are not known to be supported.
  //

  // Extended Profile (88 == 0x58).
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1.58A01E\"'"));
}

// When modifying this test, also change CodecSupportTest_Avc1Variants.
IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_Avc3Variants) {
  // avc3 without extensions results in "maybe" for compatibility.
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3\"'"));

  // Any 6-digit hexadecimal number will result in at least "maybe".
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.123456\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.ABCDEF\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.abcdef\"'"));
  EXPECT_EQ(kNot,       CanPlay("'video/mp4; codecs=\"avc3.12345\"'"));
  EXPECT_EQ(kNot,       CanPlay("'video/mp4; codecs=\"avc3.1234567\"'"));

  // Both upper and lower case hexadecimal digits are accepted.
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42e01e\"'"));

  // From a YouTube DASH MSE test manifest.
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.4d401f\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.4d401e\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.4d4015\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.640028\"'"));

  //
  // Baseline Profile (66 == 0x42).
  //  The first four digits must be 42E0, but Chrome also allows 42[8-F]0.
  //  (See http://crbug.com/408552#c17.)
  //  The last two digits must be any valid level.
  //
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42E00A\"'"));

  // The third digit must be 8-F.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc3.42001E\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc3.42101E\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc3.42201E\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc3.42301E\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc3.42401E\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc3.42501E\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc3.42601E\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc3.42701E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42801E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42901E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42A01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42B01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42C01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42D01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.42F01E\"'"));

  // The fourth digit must be 0.
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.42E11E\"'"));

  //
  // Main Profile (77 == 0x4D).
  //  The first four digits must be 4D40.
  //  The last two digits must be any valid level.
  //
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.4D400A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.4D401E\"'"));

  // Other values are not allowed for the third and fourth digits.
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.4D301E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.4D501E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.4D411E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.4D4F1E\"'"));

  //
  // High Profile (100 == 0x64).
  //  The first four digits must be 6400.
  //  The last two digits must be any valid level.
  //
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.64000A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.64001E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc3.64001F\"'"));

  // Other values are not allowed for the third and fourth digits.
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.64101E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.64f01E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.64011E\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.640F1E\"'"));

  //
  //  Other profiles are not known to be supported.
  //

  // Extended Profile (88 == 0x58).
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3.58A01E\"'"));
}

// Tests AVC levels using AVC1 Baseline (0x42E0zz).
// Other supported values for the first four hexadecimal digits should behave
// the same way but are not tested.
// For each full level, the following are tested:
// * The hexadecimal value before it is not supported.
// * The hexadecimal value for the main level and all sub-levels are supported.
// * The hexadecimal value after the last sub-level it is not supported.
// * Decimal representations of the levels are not supported.

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_AvcLevels) {
  // Level 0 is not supported.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E000\"'"));

  // Levels 1 (0x0A), 1.1 (0x0B), 1.2 (0x0C), 1.3 (0x0D).
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E009\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E00A\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E00B\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E00C\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E00D\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E00E\"'"));
  // Verify that decimal representations of levels are not supported.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E001\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E010\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E011\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E012\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E013\"'"));

  // Levels 2 (0x14), 2.1 (0x15), 2.2 (0x16)
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E013\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E014\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E015\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E016\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E017\"'"));
  // Verify that decimal representations of levels are not supported.
  // However, 20 is level 3.2.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E002\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E020\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E021\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E022\"'"));

  // Levels 3 (0x1e), 3.1 (0x1F), 3.2 (0x20)
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E01D\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E01E\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E01F\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E020\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E021\"'"));
  // Verify that decimal representations of levels are not supported.
  // However, 32 is level 5.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E003\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E030\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E031\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E032\"'"));

  // Levels 4 (0x28), 4.1 (0x29), 4.2 (0x2A)
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E027\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E028\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E029\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E02A\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E02B\"'"));
  // Verify that decimal representations of levels are not supported.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E004\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E040\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E041\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E042\"'"));

  // Levels 5 (0x32), 5.1 (0x33).
  // Note: Level 5.2 (0x34) is not considered valid (crbug.com/460376).
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E031\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E032\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"avc1.42E033\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E034\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E035\"'"));
  // Verify that decimal representations of levels are not supported.
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E005\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E050\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E051\"'"));
  EXPECT_EQ(kPropMaybe,    CanPlay("'video/mp4; codecs=\"avc1.42E052\"'"));
}

// All values that return positive results are tested. There are also
// negative tests for values around or that could potentially be confused with
// (e.g. case, truncation, hex <-> deciemal conversion) those values that return
// positive results.
IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_Mp4aVariants) {
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.\"'"));

  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.6\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.60\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.61\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.62\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.63\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.65\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.65\"'"));
  // MPEG2 AAC Main, LC, and SSR are supported except on Android.
  EXPECT_EQ(kMpeg2AacProbably, CanPlay("'audio/mp4; codecs=\"mp4a.66\"'"));
  EXPECT_EQ(kMpeg2AacProbably, CanPlay("'audio/mp4; codecs=\"mp4a.67\"'"));
  EXPECT_EQ(kMpeg2AacProbably, CanPlay("'audio/mp4; codecs=\"mp4a.68\"'"));
  // MP3.
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.69\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.6A\"'"));
  // MP3.
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.6B\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.6b\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.6C\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.6D\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.6E\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.6F\"'"));

  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.76\"'"));

  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.4\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.39\"'"));

  // mp4a.40 without further extension is ambiguous and results in "maybe".
  EXPECT_EQ(kPropMaybe,    CanPlay("'audio/mp4; codecs=\"mp4a.40\"'"));

  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.0\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.1\"'"));
  // MPEG4 AAC LC.
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.3\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.4\"'"));
  // MPEG4 AAC SBR v1.
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.5\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.6\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.7\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.8\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.9\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.10\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.20\"'"));
  // MPEG4 AAC SBR PS v2.
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.29\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.30\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.40\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.50\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.290\"'"));
  // Check conversions of decimal 29 to hex and hex 29 to decimal.
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.1d\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.1D\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.41\"'"));

  // Allow leading zeros in aud-oti for specific MPEG4 AAC strings.
  // See http://crbug.com/440607.
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.00\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.01\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.02\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.03\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.04\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.05\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.40.029\"'"));

  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.41\"'"));
  EXPECT_EQ(kNot,          CanPlay("'audio/mp4; codecs=\"mp4a.41.2\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.4.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.400.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.040.2\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.4.5\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.400.5\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp4; codecs=\"mp4a.040.5\"'"));
}

IN_PROC_BROWSER_TEST_F(MediaCanPlayTypeTest, CodecSupportTest_HLS) {
  // HLS are supported only on Android IceCreamSandwich and above (API level 14)
  std::string probablyCanPlayHLS = kNot;
  std::string maybeCanPlayHLS = kNot;
#if defined(OS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->sdk_int() > 13) {
    probablyCanPlayHLS = kProbably;
    maybeCanPlayHLS = kMaybe;
  }
#endif
  EXPECT_EQ(maybeCanPlayHLS, CanPlay("'application/x-mpegurl'"));

  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc1\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc3\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"mp4a.40\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc1, mp4a.40\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc3, mp4a.40\"'"));

  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc1.42E01E\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc3.42E01E\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc3.42801E\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc3.42C01E\"'"));

  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc1.42E11E\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc1.42101E\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc1.42701E\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc1.42F01E\"'"));

  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"mp4a.40.02\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
      CanPlay("'application/x-mpegurl; codecs=\"avc1.42E01E, mp4a.40.2\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
      CanPlay("'application/x-mpegurl; codecs=\"avc1.42E01E, mp4a.40.02\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
      CanPlay("'application/x-mpegurl; codecs=\"avc3.42E01E, mp4a.40.5\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
      CanPlay("'application/x-mpegurl; codecs=\"avc3.42E01E, mp4a.40.05\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
      CanPlay("'application/x-mpegurl; codecs=\"avc3.42E01E, mp4a.40.29\"'"));

  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc1, mp4a.40.2\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc1, mp4a.40.02\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc3, mp4a.40.2\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc3, mp4a.40.02\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
      CanPlay("'application/x-mpegurl; codecs=\"avc1.42E01E, mp4a.40\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
      CanPlay("'application/x-mpegurl; codecs=\"avc3.42E01E, mp4a.40\"'"));

  TestMPEGUnacceptableCombinations("application/x-mpegurl");

  EXPECT_EQ(maybeCanPlayHLS, CanPlay("'application/vnd.apple.mpegurl'"));

  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"mp4a.40\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
      CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1, mp4a.40\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
      CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3, mp4a.40\"'"));

  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1.42E01E\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3.42E01E\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3.42801E\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3.42C01E\"'"));

  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1.42E11E\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1.42101E\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1.42701E\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1.42F01E\"'"));

  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"mp4a.40.02\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"mp4a.40.5\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"mp4a.40.05\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"mp4a.40.29\"'"));

  EXPECT_EQ(maybeCanPlayHLS,
      CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1, mp4a.40.2\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
      CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1, mp4a.40.02\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
      CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3, mp4a.40.2\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
      CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3, mp4a.40.02\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
      CanPlay("'application/vnd.apple.mpegurl; "
              "codecs=\"avc1.42E01E, mp4a.40\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
      CanPlay("'application/vnd.apple.mpegurl; "
              "codecs=\"avc3.42E01E, mp4a.40\"'"));

  TestMPEGUnacceptableCombinations("application/vnd.apple.mpegurl");
}

}  // namespace content
