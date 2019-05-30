// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_SHARED_IMAGE_VIDEO_PROVIDER_H_
#define MEDIA_GPU_ANDROID_SHARED_IMAGE_VIDEO_PROVIDER_H_

#include "base/callback.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "media/gpu/android/codec_image_group.h"
#include "media/gpu/android/promotion_hint_aggregator.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
struct SyncToken;
}  // namespace gpu

namespace media {

// Provider class for shared images.
class MEDIA_GPU_EXPORT SharedImageVideoProvider {
 public:
  // Description of the underlying properties of the shared image.
  struct ImageSpec {
    ImageSpec(gfx::Size size, scoped_refptr<CodecImageGroup> group);
    ImageSpec(const ImageSpec&);
    ~ImageSpec();

    // Size of the underlying texture.
    gfx::Size size;

    // Image group to which the CodecImage should belong.  In other words, this
    // indicates that the image will share the same underlying TextureOwner.
    scoped_refptr<CodecImageGroup> image_group;

    // TODO: Include other properties, if they matter, like texture format.

    bool operator==(const ImageSpec&);
  };

  using ReleaseCB = base::OnceCallback<void(const gpu::SyncToken&)>;

  // Description of the image that's being provided to the client.
  struct ImageRecord {
    ImageRecord();
    ImageRecord(ImageRecord&&);
    ~ImageRecord();

    // Mailbox to which this shared image is bound.
    gpu::Mailbox mailbox;

    // Sampler conversion information which is used in vulkan context.
    base::Optional<gpu::VulkanYCbCrInfo> ycbcr_info;

    // Release callback.  When this is called (or dropped), the image will be
    // considered unused.
    ReleaseCB release_cb;

   private:
    DISALLOW_COPY_AND_ASSIGN(ImageRecord);
  };

  SharedImageVideoProvider() = default;
  virtual ~SharedImageVideoProvider() = default;

  using ImageReadyCB = base::OnceCallback<void(ImageRecord)>;

  // Call |cb| when we have a shared image that matches |spec|.  We may call
  // |cb| back before returning, or we might post it for later.
  // |output_buffer|, |texture_owner|, and |promotion_hint_cb| probably should
  // not be provided to the provider, but need to be refactored a bit first.
  // They will be used to configure the SharedImageVideo to display the buffer
  // on the given texture, and return promotion hints.
  virtual void RequestImage(
      ImageReadyCB cb,
      const ImageSpec& spec,
      std::unique_ptr<CodecOutputBuffer> output_buffer,
      scoped_refptr<TextureOwner> texture_owner,
      PromotionHintAggregator::NotifyPromotionHintCB promotion_hint_cb) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedImageVideoProvider);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_SHARED_IMAGE_VIDEO_PROVIDER_H_
