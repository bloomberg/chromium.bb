// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/resources/returned_resource.h"

namespace viz {

TransferableResource::TransferableResource()
    : id(0),
      format(RGBA_8888),
      buffer_format(gfx::BufferFormat::RGBA_8888),
      filter(0),
      read_lock_fences_enabled(false),
      is_software(false),
      shared_bitmap_sequence_number(0),
#if defined(OS_ANDROID)
      is_backed_by_surface_texture(false),
      wants_promotion_hint(false),
#endif
      is_overlay_candidate(false) {
}

TransferableResource::TransferableResource(const TransferableResource& other) =
    default;

TransferableResource::~TransferableResource() {}

ReturnedResource TransferableResource::ToReturnedResource() const {
  ReturnedResource returned;
  returned.id = id;
  returned.sync_token = mailbox_holder.sync_token;
  returned.count = 1;
  return returned;
}

// static
std::vector<ReturnedResource> TransferableResource::ReturnResources(
    const std::vector<TransferableResource>& input) {
  std::vector<ReturnedResource> out;
  out.reserve(input.size());
  for (const auto& r : input)
    out.push_back(r.ToReturnedResource());
  return out;
}

}  // namespace viz
