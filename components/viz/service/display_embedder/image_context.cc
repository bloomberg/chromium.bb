// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/image_context.h"

#include "gpu/command_buffer/service/shared_image_representation.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"

namespace viz {

ImageContext::ImageContext(const gpu::Mailbox& mailbox,
                           const gfx::Size& size,
                           ResourceFormat resource_format,
                           sk_sp<SkColorSpace> color_space,
                           SkAlphaType alpha_type,
                           GrSurfaceOrigin origin,
                           const gpu::SyncToken& sync_token)
    : mailbox(mailbox),
      size(size),
      resource_format(resource_format),
      color_space(std::move(color_space)),
      alpha_type(alpha_type),
      origin(origin),
      sync_token(sync_token) {}

ImageContext::ImageContext(const ResourceMetadata& metadata)
    : ImageContext(metadata.mailbox_holder.mailbox,
                   metadata.size,
                   metadata.resource_format,
                   metadata.color_space.ToSkColorSpace(),
                   metadata.alpha_type,
                   metadata.origin,
                   metadata.mailbox_holder.sync_token) {}

ImageContext::ImageContext(RenderPassId render_pass_id,
                           const gfx::Size& size,
                           ResourceFormat resource_format,
                           bool mipmap,
                           sk_sp<SkColorSpace> color_space)
    : render_pass_id(render_pass_id),
      size(size),
      resource_format(resource_format),
      mipmap(mipmap ? GrMipMapped::kYes : GrMipMapped::kNo),
      color_space(std::move(color_space)) {}

ImageContext::~ImageContext() {
  DCHECK(!representation_is_being_accessed);
}

}  // namespace viz
