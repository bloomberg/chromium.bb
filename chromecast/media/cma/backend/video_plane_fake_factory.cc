// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video_plane_fake.h"

namespace chromecast {
namespace media {

// Global accessor to the video plane.
VideoPlane* GetVideoPlane() {
  return VideoPlaneFake::GetInstance();
}

}  // namespace media
}  // namespace chromecast
