// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video_plane_fake.h"

namespace chromecast {
namespace media {

scoped_ptr<VideoPlane> CreateVideoPlane() {
  return make_scoped_ptr(new VideoPlaneFake());
}

}  // namespace media
}  // namespace chromecast
