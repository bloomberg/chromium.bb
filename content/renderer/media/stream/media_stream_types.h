// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_TYPES_H_
#define CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_TYPES_H_

namespace content {

using VideoTrackSettingsCallback =
    base::RepeatingCallback<void(int width, int height, double frame_rate)>;

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_TYPES_H_
