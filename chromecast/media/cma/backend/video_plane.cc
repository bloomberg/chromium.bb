// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/video_plane.h"

#include "base/memory/singleton.h"

namespace chromecast {
namespace media {

VideoPlane::VideoPlane() {
}

VideoPlane::~VideoPlane() {
}

class VideoPlaneRegistry {
 public:
  static VideoPlaneRegistry* GetInstance() {
    return Singleton<VideoPlaneRegistry>::get();
  }

  VideoPlane* GetVideoPlane();

 private:
  friend struct DefaultSingletonTraits<VideoPlaneRegistry>;
  friend class Singleton<VideoPlaneRegistry>;

  VideoPlaneRegistry();
  virtual ~VideoPlaneRegistry();

  scoped_ptr<VideoPlane> video_plane_;

  DISALLOW_COPY_AND_ASSIGN(VideoPlaneRegistry);
};

VideoPlaneRegistry::VideoPlaneRegistry() :
  video_plane_(CreateVideoPlane()) {
}

VideoPlaneRegistry::~VideoPlaneRegistry() {
}

VideoPlane* VideoPlaneRegistry::GetVideoPlane() {
  return video_plane_.get();
}

VideoPlane* GetVideoPlane() {
  return VideoPlaneRegistry::GetInstance()->GetVideoPlane();
}

}  // namespace media
}  // namespace chromecast
