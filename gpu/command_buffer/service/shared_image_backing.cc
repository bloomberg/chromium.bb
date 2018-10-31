// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing.h"

#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {

SharedImageBacking::SharedImageBacking(const Mailbox& mailbox,
                                       viz::ResourceFormat format,
                                       const gfx::Size& size,
                                       const gfx::ColorSpace& color_space,
                                       uint32_t usage,
                                       size_t estimated_size)
    : mailbox_(mailbox),
      format_(format),
      size_(size),
      color_space_(color_space),
      usage_(usage),
      estimated_size_(estimated_size) {}

SharedImageBacking::~SharedImageBacking() = default;

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                     MemoryTypeTracker* tracker) {
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                MemoryTypeTracker* tracker) {
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationSkia> SharedImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return nullptr;
}

void SharedImageBacking::AddRef(SharedImageRepresentation* representation) {
  bool first_ref = refs_.empty();
  refs_.push_back(representation);

  if (first_ref) {
    refs_[0]->tracker()->TrackMemAlloc(estimated_size_);
  }
}

void SharedImageBacking::ReleaseRef(SharedImageRepresentation* representation) {
  auto found = std::find(refs_.begin(), refs_.end(), representation);
  DCHECK(found != refs_.end());

  // If the found representation is the first (owning) ref, free the attributed
  // memory.
  bool released_owning_ref = found == refs_.begin();
  if (released_owning_ref)
    (*found)->tracker()->TrackMemFree(estimated_size_);

  refs_.erase(found);

  if (!released_owning_ref)
    return;

  if (!refs_.empty()) {
    refs_[0]->tracker()->TrackMemAlloc(estimated_size_);
    return;
  }

  // Last ref deleted, clean up.
  Destroy();
}

}  // namespace gpu
