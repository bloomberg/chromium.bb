// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TRANSFERABLE_RESOURCE_H_
#define CC_RESOURCES_TRANSFERABLE_RESOURCE_H_

#include <stdint.h>

#include <vector>

#include "cc/base/resource_id.h"
#include "cc/cc_export.h"
#include "components/viz/common/quads/resource_format.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
struct ReturnedResource;

struct CC_EXPORT TransferableResource {
  TransferableResource();
  TransferableResource(const TransferableResource& other);
  ~TransferableResource();

  ReturnedResource ToReturnedResource() const;
  static std::vector<ReturnedResource> ReturnResources(
      const std::vector<TransferableResource>& input);

  ResourceId id;
  // Refer to ResourceProvider::Resource for the meaning of the following data.
  viz::ResourceFormat format;
  gfx::BufferFormat buffer_format;
  uint32_t filter;
  gfx::Size size;
  gpu::MailboxHolder mailbox_holder;
  bool read_lock_fences_enabled;
  bool is_software;
  uint32_t shared_bitmap_sequence_number;
#if defined(OS_ANDROID)
  bool is_backed_by_surface_texture;
  bool wants_promotion_hint;
#endif
  bool is_overlay_candidate;
  gfx::ColorSpace color_space;
};

}  // namespace cc

#endif  // CC_RESOURCES_TRANSFERABLE_RESOURCE_H_
