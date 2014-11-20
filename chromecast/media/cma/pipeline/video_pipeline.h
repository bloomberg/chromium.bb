// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_VIDEO_PIPELINE_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_VIDEO_PIPELINE_H_

#include "base/macros.h"

namespace chromecast {
namespace media {
struct VideoPipelineClient;

class VideoPipeline {
 public:
  VideoPipeline();
  virtual ~VideoPipeline();

  virtual void SetClient(const VideoPipelineClient& client) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoPipeline);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_VIDEO_PIPELINE_H_
