// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_HOLE_FRAME_FACTORY_H_
#define CHROMECAST_RENDERER_MEDIA_HOLE_FRAME_FACTORY_H_

#include <GLES2/gl2.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"

namespace gfx {
class Size;
}

namespace media {
class GpuVideoAcceleratorFactories;
class VideoFrame;
}

namespace chromecast {
namespace media {

// Creates VideoFrames for CMA - native textures that get turned into
// transparent holes in the browser compositor using overlay system.
// All calls (including ctor/dtor) must be on media thread.
class HoleFrameFactory {
 public:
  explicit HoleFrameFactory(
      ::media::GpuVideoAcceleratorFactories* gpu_factories);
  ~HoleFrameFactory();

  scoped_refptr<::media::VideoFrame> CreateHoleFrame(const gfx::Size& size);

 private:
  ::media::GpuVideoAcceleratorFactories* gpu_factories_;
  gpu::Mailbox mailbox_;
  gpu::SyncToken sync_token_;
  GLuint texture_;
  GLuint image_id_;

  DISALLOW_COPY_AND_ASSIGN(HoleFrameFactory);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_HOLE_FRAME_FACTORY_H_
