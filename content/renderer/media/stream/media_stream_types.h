// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_TYPES_H_
#define CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_TYPES_H_

#include "media/capture/video_capture_types.h"

namespace content {

using VideoTrackSettingsCallback =
    base::RepeatingCallback<void(gfx::Size frame_size, double frame_rate)>;

using VideoTrackFormatCallback =
    base::RepeatingCallback<void(const media::VideoCaptureFormat&)>;

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_TYPES_H_
