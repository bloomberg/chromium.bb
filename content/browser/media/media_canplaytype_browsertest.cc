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

// TODO(amogh.bihani): Change the opus tests when opus is  on
// Android. (http://crbug.com/318436).
#if !defined(OS_ANDROID)
const char kOggVideoProbably[] = "probably";
const char kOggVideoMaybe[] = "maybe";
const char kTheoraProbably[] = "probably";
const char kOpusProbably[] = "probably";
#else
const char kOggVideoProbably[] = "";
const char kOggVideoMaybe[] = "";
const char kTheoraProbably[] = "";
const char kOpusProbably[] = "";
#endif  // !OS_ANDROID

namespace content {

class MediaCanPlayTypeTest : public MediaBrowserTest {
public:
  MediaCanPlayTypeTest() : url_("about:blank") { }

  virtual void SetUpOnMainThread() OVERRIDE {
    NavigateToURL(shell(), url_);
  }

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

  void TestMPEGUnacceptableCombinations(std::string mime) {
    // Codecs must be followed by valid hexadecimal number.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.unknown\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.unknown\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.unknown\"'"));

    // Codecs must not end with a dot.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4a.40.\"'"));

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

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1.4D401E, 1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3.64001F, 1\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora, mp4a\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"theora, mp4a.40.2\"'"));

    // Codecs are case sensitive.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC1\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC1.4d401e\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC3\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC3.64001f\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"MP4A\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"MP4A.40.2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC1, MP4\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"AVC3, MP4\"'"));
    EXPECT_EQ(kNot,
              CanPlay("'" + mime + "; codecs=\", AVC1.4D401E, MP4.40.2\"'"));
    EXPECT_EQ(kNot,
              CanPlay("'" + mime + "; codecs=\", AVC3.64001F, MP4.40.2\"'"));

    // Unknown codecs.
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc2\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc4\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc1x\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"avc3x\"'"));
    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"mp4ax\"'"));

    EXPECT_EQ(kNot, CanPlay("'" + mime + "; codecs=\"unknown\"'"));
  }

  void TestOGGUnacceptableCombinations(std::string mime) {
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
    EXPECT_EQ(kNot, CanPlay("'" + mime +"; codecs=\"theora, mp4a.40.2\"'"));

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

  void TestWEBMUnacceptableCombinations(std::string mime) {
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

  void TestWAVUnacceptableCombinations(std::string mime) {
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
  // On Android, VP9 is supported only on KitKat and above (API level 19).
  std::string VP9Probably = "probably";
  std::string VP9AndOpusProbably = "probably";
#if defined(OS_ANDROID)
  VP9AndOpusProbably = "";
  if (base::android::BuildInfo::GetInstance()->sdk_int() < 19)
    VP9Probably = "";
#endif
  EXPECT_EQ(kMaybe, CanPlay("'video/webm'"));

  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8.0\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8, vorbis\"'"));
  EXPECT_EQ(kProbably, CanPlay("'video/webm; codecs=\"vp8.0, vorbis\"'"));
  EXPECT_EQ(kOpusProbably, CanPlay("'video/webm; codecs=\"vp8, opus\"'"));
  EXPECT_EQ(kOpusProbably, CanPlay("'video/webm; codecs=\"vp8.0, opus\"'"));

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
  EXPECT_EQ(kOpusProbably, CanPlay("'audio/webm; codecs=\"opus\"'"));
  EXPECT_EQ(kOpusProbably, CanPlay("'audio/webm; codecs=\"opus, vorbis\"'"));

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
  EXPECT_EQ(kOpusProbably, CanPlay("'audio/ogg; codecs=\"opus\"'"));
  EXPECT_EQ(kOpusProbably, CanPlay("'audio/ogg; codecs=\"vorbis, opus\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"theora\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"theora, opus\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/ogg; codecs=\"theora, vorbis\"'"));

  TestOGGUnacceptableCombinations("audio/ogg");

  EXPECT_EQ(kMaybe, CanPlay("'application/ogg'"));
  EXPECT_EQ(kProbably, CanPlay("'application/ogg; codecs=\"vorbis\"'"));
  EXPECT_EQ(kTheoraProbably, CanPlay("'application/ogg; codecs=\"theora\"'"));
  EXPECT_EQ(kOpusProbably, CanPlay("'application/ogg; codecs=\"opus\"'"));
  EXPECT_EQ(kTheoraProbably,
            CanPlay("'application/ogg; codecs=\"theora, vorbis\"'"));
  EXPECT_EQ(kTheoraProbably,
            CanPlay("'application/ogg; codecs=\"theora, opus\"'"));
  EXPECT_EQ(kOpusProbably,
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

  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mpeg; codecs=\"mp4a.40.2\"'"));

  TestMPEGUnacceptableCombinations("audio/mpeg");

  // audio/mp3 does not allow any codecs parameter
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp3'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/mp3; codecs=\"mp4a.40.2\"'"));

  TestMPEGUnacceptableCombinations("audio/mp3");

  // audio/x-mp3 does not allow any codecs parameter
  EXPECT_EQ(kPropProbably, CanPlay("'audio/x-mp3'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc3\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"avc3.64001F\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-mp3; codecs=\"mp4a.40.2\"'"));

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
  EXPECT_EQ(kPropProbably, CanPlay("'video/mp4; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc1.42E01E, mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/mp4; codecs=\"avc3.42E01E, mp4a.40.5\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc1, mp4a.40.2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/mp4; codecs=\"avc3, mp4a.40.2\"'"));
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
  EXPECT_EQ(kPropProbably, CanPlay("'video/x-m4v; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc1.42E01E, mp4a.40.2\"'"));
  EXPECT_EQ(kPropProbably,
            CanPlay("'video/x-m4v; codecs=\"avc3.42E01E, mp4a.40.5\"'"));

  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc1, mp4a.40.2\"'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'video/x-m4v; codecs=\"avc3, mp4a.40.2\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'video/x-m4v; codecs=\"avc1.42E01E, mp4a.40\"'"));
  EXPECT_EQ(kPropMaybe,
            CanPlay("'video/x-m4v; codecs=\"avc3.42E01E, mp4a.40\"'"));

  TestMPEGUnacceptableCombinations("video/x-m4v");

  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4'"));
  EXPECT_EQ(kPropMaybe, CanPlay("'audio/mp4; codecs=\"mp4a.40\"'"));
  EXPECT_EQ(kPropProbably, CanPlay("'audio/mp4; codecs=\"mp4a.40.2\"'"));

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

  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc1\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc3\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc1, mp4a\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc3, mp4a\"'"));

  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc1.4D401E\"'"));
  EXPECT_EQ(kNot, CanPlay("'audio/x-m4a; codecs=\"avc3.64001F\"'"));

  TestMPEGUnacceptableCombinations("audio/x-m4a");
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
            CanPlay("'application/x-mpegurl; codecs=\"mp4a.40.2\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
      CanPlay("'application/x-mpegurl; codecs=\"avc1.42E01E, mp4a.40.2\"'"));
  EXPECT_EQ(probablyCanPlayHLS,
      CanPlay("'application/x-mpegurl; codecs=\"avc3.42E01E, mp4a.40.5\"'"));

  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc1, mp4a.40.2\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
            CanPlay("'application/x-mpegurl; codecs=\"avc3, mp4a.40.2\"'"));
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
            CanPlay("'application/vnd.apple.mpegurl; codecs=\"mp4a.40.2\"'"));

  EXPECT_EQ(maybeCanPlayHLS,
      CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc1, mp4a.40.2\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
      CanPlay("'application/vnd.apple.mpegurl; codecs=\"avc3, mp4a.40.2\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
      CanPlay("'application/vnd.apple.mpegurl; "
              "codecs=\"avc1.42E01E, mp4a.40\"'"));
  EXPECT_EQ(maybeCanPlayHLS,
      CanPlay("'application/vnd.apple.mpegurl; "
              "codecs=\"avc3.42E01E, mp4a.40\"'"));

  TestMPEGUnacceptableCombinations("application/vnd.apple.mpegurl");
}

}  // namespace content
