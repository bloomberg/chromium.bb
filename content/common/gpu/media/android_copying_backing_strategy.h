// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_ANDROID_COPYING_BACKING_STRATEGY_H_
#define CONTENT_COMMON_GPU_MEDIA_ANDROID_COPYING_BACKING_STRATEGY_H_

#include "base/compiler_specific.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/common/gpu/media/android_video_decode_accelerator.h"

namespace gpu {
class CopyTextureCHROMIUMResourceManager;
}

namespace media {
class PictureBuffer;
}

namespace content {

class AndroidVideoDecodeAcceleratorStateProvider;

// A BackingStrategy implementation that copies images to PictureBuffer
// textures via gpu texture copy.
class CONTENT_EXPORT AndroidCopyingBackingStrategy
    : public AndroidVideoDecodeAccelerator::BackingStrategy {
 public:
  AndroidCopyingBackingStrategy();
  ~AndroidCopyingBackingStrategy() override;

  // AndroidVideoDecodeAccelerator::BackingStrategy
  void SetStateProvider(AndroidVideoDecodeAcceleratorStateProvider*) override;
  void Cleanup() override;
  uint32 GetNumPictureBuffers() const override;
  uint32 GetTextureTarget() const override;
  void AssignCurrentSurfaceToPictureBuffer(
      int32 codec_buffer_index,
      const media::PictureBuffer&) override;

 private:
  // Used for copy the texture from surface texture to picture buffers.
  scoped_ptr<gpu::CopyTextureCHROMIUMResourceManager> copier_;

  AndroidVideoDecodeAcceleratorStateProvider* state_provider_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_ANDROID_COPYING_BACKING_STRATEGY_H_
