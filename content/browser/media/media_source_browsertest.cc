// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_browsertest.h"

// Common media types.
static const char kWebMAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
static const char kWebMVideoOnly[] = "video/webm; codecs=\"vp8\"";
static const char kWebMAudioVideo[] = "video/webm; codecs=\"vorbis, vp8\"";

namespace content {

class MediaSourceTest : public content::MediaBrowserTest {
 public:
  void TestSimplePlayback(const char* media_file, const char* media_type,
                          const char* expectation) {
    std::vector<StringPair> query_params;
    query_params.push_back(std::make_pair("mediafile", media_file));
    query_params.push_back(std::make_pair("mediatype", media_type));
    RunMediaTestPage("media_source_player.html", &query_params, expectation,
                     true);
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

// Flaky test crbug.com/246308
IN_PROC_BROWSER_TEST_F(MediaSourceTest, DISABLED_ConfigChangeVideo) {
  RunMediaTestPage("mse_config_change.html", NULL, kEnded, true);
}

}  // namespace content