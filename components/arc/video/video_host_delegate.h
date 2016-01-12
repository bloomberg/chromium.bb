// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VIDEO_VIDEO_HOST_DELEGATE_H
#define COMPONENTS_ARC_VIDEO_VIDEO_HOST_DELEGATE_H

#include "components/arc/common/video.mojom.h"

namespace arc {

// VideoHostDelegate is an abstract class providing video decoding/encoding
// acceleration service.
//
// The purpose of this interface is to create a channel into the GPU process
// only, which then handles video decoding/encoding, without going through
// VideoHostDelegate anymore.
class VideoHostDelegate : public VideoHost {
 public:
  // Called when the video service is stopping.
  virtual void OnStopping() = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_VIDEO_VIDEO_HOST_DELEGATE_H
