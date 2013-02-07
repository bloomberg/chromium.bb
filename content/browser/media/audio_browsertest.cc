// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/layout_browsertest.h"

namespace content {

class AudioLayoutTest : public InProcessBrowserLayoutTest {
 protected:
  AudioLayoutTest() : InProcessBrowserLayoutTest(
      base::FilePath(), base::FilePath().AppendASCII("media")) {
  }
};

IN_PROC_BROWSER_TEST_F(AudioLayoutTest, AudioConstructorPreload) {
  RunLayoutTest("audio-constructor-preload.html");
}

IN_PROC_BROWSER_TEST_F(AudioLayoutTest, AudioConstructor) {
  RunLayoutTest("audio-constructor.html");
}

IN_PROC_BROWSER_TEST_F(AudioLayoutTest, AudioConstructorSrc) {
  RunLayoutTest("audio-constructor-src.html");
}

IN_PROC_BROWSER_TEST_F(AudioLayoutTest, AudioDataUrl) {
  RunLayoutTest("audio-data-url.html");
}

// The test fails since there is no real audio device on the build bots to get
// the ended event fired. Should pass once we run it on bots with audio devices.
IN_PROC_BROWSER_TEST_F(AudioLayoutTest, DISABLED_AudioGarbageCollect) {
  RunLayoutTest("audio-garbage-collect.html");
}

IN_PROC_BROWSER_TEST_F(AudioLayoutTest, AudioNoInstalledEngines) {
  RunLayoutTest("audio-no-installed-engines.html");
}

#if defined(OS_CHROMEOS) && defined(USE_AURA)
// http://crbug.com/115530
#define MAYBE_AudioOnlyVideoIntrinsicSize DISABLED_AudioOnlyVideoIntrinsicSize
#else
#define MAYBE_AudioOnlyVideoIntrinsicSize AudioOnlyVideoIntrinsicSize
#endif

IN_PROC_BROWSER_TEST_F(AudioLayoutTest, MAYBE_AudioOnlyVideoIntrinsicSize) {
  RunLayoutTest("audio-only-video-intrinsic-size.html");
}

// http://crbug.com/167640
IN_PROC_BROWSER_TEST_F(AudioLayoutTest, DISABLED_AudioPlayEvent) {
  RunLayoutTest("audio-play-event.html");
}

IN_PROC_BROWSER_TEST_F(AudioLayoutTest, MediaCanPlayWavAudio) {
  RunLayoutTest("media-can-play-wav-audio.html");
}

IN_PROC_BROWSER_TEST_F(AudioLayoutTest, MediaDocumentAudioSize) {
  RunLayoutTest("media-document-audio-size.html");
}

}  // namespace content
