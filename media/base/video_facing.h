// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FACING_H_
#define MEDIA_BASE_VIDEO_FACING_H_

namespace media {

// Facing mode for video capture.
enum VideoFacingMode {
  MEDIA_VIDEO_FACING_NONE = 0,
  MEDIA_VIDEO_FACING_USER,
  MEDIA_VIDEO_FACING_ENVIRONMENT,

  NUM_MEDIA_VIDEO_FACING_MODE
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FACING_H_
