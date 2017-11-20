// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_TRANSFERABLE_RESOURCE_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_TRANSFERABLE_RESOURCE_H_

#include <stdint.h>

#include <vector>

#include "build/build_config.h"
#include "components/viz/common/quads/shared_bitmap.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

struct ReturnedResource;

struct VIZ_COMMON_EXPORT TransferableResource {
  TransferableResource();
  TransferableResource(const TransferableResource& other);
  ~TransferableResource();

  ReturnedResource ToReturnedResource() const;
  static std::vector<ReturnedResource> ReturnResources(
      const std::vector<TransferableResource>& input);

  static TransferableResource MakeSoftware(const SharedBitmapId& id,
                                           uint32_t sequence_number,
                                           const gfx::Size& size) {
    TransferableResource r;
    r.is_software = true;
    r.mailbox_holder.mailbox = id;
    r.shared_bitmap_sequence_number = sequence_number;
    r.size = size;
    return r;
  }

  static TransferableResource MakeGL(const gpu::Mailbox& mailbox,
                                     uint32_t filter,
                                     uint32_t texture_target,
                                     const gpu::SyncToken& sync_token) {
    TransferableResource r;
    r.is_software = false;
    r.filter = filter;
    r.mailbox_holder.mailbox = mailbox;
    r.mailbox_holder.texture_target = texture_target;
    r.mailbox_holder.sync_token = sync_token;
    r.size = gfx::Size();
    return r;
  }

  static TransferableResource MakeGLOverlay(const gpu::Mailbox& mailbox,
                                            uint32_t filter,
                                            uint32_t texture_target,
                                            const gpu::SyncToken& sync_token,
                                            const gfx::Size& size,
                                            bool is_overlay_candidate) {
    TransferableResource r =
        MakeGL(mailbox, filter, texture_target, sync_token);
    r.size = size;
    r.is_overlay_candidate = is_overlay_candidate;
    return r;
  }

  // TODO(danakj): Some of these fiends are only GL, some are only Software,
  // some are both but used for different purposes (like the mailbox name).
  // It would be nice to group things together and make it more clear when
  // they will be used or not, and provide easier access to fields such as the
  // mailbox that also show the intent for software for GL.

  ResourceId id;
  // Refer to ResourceProvider::Resource for the meaning of the following data.
  ResourceFormat format;
  gfx::BufferFormat buffer_format;
  uint32_t filter;
  gfx::Size size;
  // The |mailbox| inside here holds the gpu::Mailbox when this is a gpu
  // resource, or the SharedBitmapId when it is a software resource.
  // The |texture_target| and sync_token| inside here only apply for gpu
  // resources.
  gpu::MailboxHolder mailbox_holder;
  bool read_lock_fences_enabled;
  bool is_software;
  uint32_t shared_bitmap_sequence_number;
#if defined(OS_ANDROID)
  // Indicates whether this resource may not be overlayed on Android, since
  // it's not backed by a SurfaceView.  This may be set in combination with
  // |is_overlay_candidate|, to find out if switching the resource to a
  // a SurfaceView would result in overlay promotion.  It's good to find this
  // out in advance, since one has no fallback path for displaying a
  // SurfaceView except via promoting it to an overlay.  Ideally, one _could_
  // promote SurfaceTexture via the overlay path, even if one ended up just
  // drawing a quad in the compositor.  However, for now, we use this flag to
  // refuse to promote so that the compositor will draw the quad.
  bool is_backed_by_surface_texture;
  // Indicates that this resource would like a promotion hint.
  bool wants_promotion_hint;
#endif
  bool is_overlay_candidate;
  gfx::ColorSpace color_space;

  bool operator==(const TransferableResource& o) const {
    return id == o.id && format == o.format &&
           buffer_format == o.buffer_format && filter == o.filter &&
           size == o.size &&
           mailbox_holder.mailbox == o.mailbox_holder.mailbox &&
           mailbox_holder.sync_token == o.mailbox_holder.sync_token &&
           mailbox_holder.texture_target == o.mailbox_holder.texture_target &&
           read_lock_fences_enabled == o.read_lock_fences_enabled &&
           is_software == o.is_software &&
           shared_bitmap_sequence_number == o.shared_bitmap_sequence_number &&
#if defined(OS_ANDROID)
           is_backed_by_surface_texture == o.is_backed_by_surface_texture &&
           wants_promotion_hint == o.wants_promotion_hint &&
#endif
           is_overlay_candidate == o.is_overlay_candidate &&
           color_space == o.color_space;
  }
  bool operator!=(const TransferableResource& o) const { return !(*this == o); }
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_TRANSFERABLE_RESOURCE_H_
