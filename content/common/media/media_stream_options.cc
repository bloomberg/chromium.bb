// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_stream_options.h"

namespace content {

const char kMediaStreamSourceTab[] = "tab";
const char kMediaStreamSourceScreen[] = "screen";
const char kMediaStreamSourceDesktop[] = "desktop";
const char kMediaStreamSourceSystem[] = "system";

TrackControls::TrackControls()
    : requested(false) {}

TrackControls::TrackControls(bool request)
    : requested(request) {}

TrackControls::TrackControls(const TrackControls& other) = default;

TrackControls::~TrackControls() {}

StreamControls::StreamControls()
    : audio(false),
      video(false),
      hotword_enabled(false),
      disable_local_echo(false) {}

StreamControls::StreamControls(bool request_audio, bool request_video)
    : audio(request_audio),
      video(request_video),
      hotword_enabled(false),
      disable_local_echo(false) {}

StreamControls::~StreamControls() {}

}  // namespace content
