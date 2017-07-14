// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/transferable_resource_struct_traits.h"
#include "gpu/ipc/common/mailbox_holder_struct_traits.h"
#include "gpu/ipc/common/mailbox_struct_traits.h"
#include "gpu/ipc/common/sync_token_struct_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

// static
bool StructTraits<cc::mojom::TransferableResourceDataView,
                  cc::TransferableResource>::
    Read(cc::mojom::TransferableResourceDataView data,
         cc::TransferableResource* out) {
  if (!data.ReadSize(&out->size) ||
      !data.ReadMailboxHolder(&out->mailbox_holder) ||
      !data.ReadColorSpace(&out->color_space))
    return false;
  out->id = data.id();
  out->format = static_cast<viz::ResourceFormat>(data.format());
  out->buffer_format = static_cast<gfx::BufferFormat>(data.buffer_format());
  out->filter = data.filter();
  out->read_lock_fences_enabled = data.read_lock_fences_enabled();
  out->is_software = data.is_software();
  out->shared_bitmap_sequence_number = data.shared_bitmap_sequence_number();
  out->is_overlay_candidate = data.is_overlay_candidate();
#if defined(OS_ANDROID)
  out->is_backed_by_surface_texture = data.is_backed_by_surface_texture();
  out->wants_promotion_hint = data.wants_promotion_hint();
#endif
  return true;
}

}  // namespace mojo
