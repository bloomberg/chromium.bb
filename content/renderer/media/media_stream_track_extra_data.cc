// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_track_extra_data.h"

#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

MediaStreamTrackExtraData::MediaStreamTrackExtraData(
    webrtc::MediaStreamTrackInterface* track)
    : track_(track) {
}

MediaStreamTrackExtraData::~MediaStreamTrackExtraData() {
}

}  // namespace content
