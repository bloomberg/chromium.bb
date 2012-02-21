// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "chrome/test/ui/ui_layout_test.h"

namespace {
  const char* kResources[] = {
    "content",
    "media-file.js",
    "video-test.js",
  };
}  // anonymous namespace

class AudioUILayoutTest : public UILayoutTest {
  protected:
    virtual ~AudioUILayoutTest() { }

    void RunMediaLayoutTest(const std::string& test_case_file_name) {
      FilePath test_dir;
      FilePath media_test_dir;
      media_test_dir = media_test_dir.AppendASCII("media");
      InitializeForLayoutTest(test_dir, media_test_dir, kNoHttpPort);

      // Copy resources first.
      for (size_t i = 0; i < arraysize(kResources); ++i) {
        AddResourceForLayoutTest(
            test_dir, media_test_dir.AppendASCII(kResources[i]));
      }

      printf("Test: %s\n", test_case_file_name.c_str());
      RunLayoutTest(test_case_file_name, kNoHttpPort);
    }
};


TEST_F(AudioUILayoutTest, AudioConstructorPreload) {
  RunMediaLayoutTest("audio-constructor-preload.html");
}

TEST_F(AudioUILayoutTest, AudioConstructor) {
  RunMediaLayoutTest("audio-constructor.html");
}

TEST_F(AudioUILayoutTest, AudioConstructorSrc) {
  RunMediaLayoutTest("audio-constructor-src.html");
}

TEST_F(AudioUILayoutTest, AudioDataUrl) {
  RunMediaLayoutTest("audio-data-url.html");
}

// The test fails since there is no real audio device on the build bots to get
// the ended event fired. Should pass once we run it on bots with audio devices.
TEST_F(AudioUILayoutTest, DISABLED_AudioGarbageCollect) {
  RunMediaLayoutTest("audio-garbage-collect.html");
}

TEST_F(AudioUILayoutTest, AudioNoInstalledEngines) {
  RunMediaLayoutTest("audio-no-installed-engines.html");
}

TEST_F(AudioUILayoutTest, AudioOnlyVideoIntrinsicSize) {
  RunMediaLayoutTest("audio-only-video-intrinsic-size.html");
}

TEST_F(AudioUILayoutTest, AudioPlayEvent) {
  RunMediaLayoutTest("audio-play-event.html");
}

TEST_F(AudioUILayoutTest, MediaCanPlayWavAudio) {
  RunMediaLayoutTest("media-can-play-wav-audio.html");
}

TEST_F(AudioUILayoutTest, MediaDocumentAudioSize) {
  RunMediaLayoutTest("media-document-audio-size.html");
}

