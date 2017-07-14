// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_TRANSFERABLE_RESOURCE_STRUCT_TRAITS_H_
#define CC_IPC_TRANSFERABLE_RESOURCE_STRUCT_TRAITS_H_

#include "cc/ipc/transferable_resource.mojom-shared.h"
#include "cc/resources/transferable_resource.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::TransferableResourceDataView,
                    cc::TransferableResource> {
  static uint32_t id(const cc::TransferableResource& resource) {
    return resource.id;
  }

  static cc::mojom::ResourceFormat format(
      const cc::TransferableResource& resource) {
    return static_cast<cc::mojom::ResourceFormat>(resource.format);
  }

  static gfx::mojom::BufferFormat buffer_format(
      const cc::TransferableResource& resource) {
    return static_cast<gfx::mojom::BufferFormat>(resource.buffer_format);
  }

  static uint32_t filter(const cc::TransferableResource& resource) {
    return resource.filter;
  }

  static gfx::Size size(const cc::TransferableResource& resource) {
    return resource.size;
  }

  static const gpu::MailboxHolder& mailbox_holder(
      const cc::TransferableResource& resource) {
    return resource.mailbox_holder;
  }

  static bool read_lock_fences_enabled(
      const cc::TransferableResource& resource) {
    return resource.read_lock_fences_enabled;
  }

  static bool is_software(const cc::TransferableResource& resource) {
    return resource.is_software;
  }

  static uint32_t shared_bitmap_sequence_number(
      const cc::TransferableResource& resource) {
    return resource.shared_bitmap_sequence_number;
  }

  static bool is_overlay_candidate(const cc::TransferableResource& resource) {
    return resource.is_overlay_candidate;
  }

  static bool is_backed_by_surface_texture(
      const cc::TransferableResource& resource) {
#if defined(OS_ANDROID)
    // TransferableResource has this in an #ifdef, but mojo doesn't let us.
    // TODO(https://crbug.com/671901)
    return resource.is_backed_by_surface_texture;
#else
    return false;
#endif
  }

  static bool wants_promotion_hint(const cc::TransferableResource& resource) {
#if defined(OS_ANDROID)
    // TransferableResource has this in an #ifdef, but mojo doesn't let us.
    // TODO(https://crbug.com/671901)
    return resource.wants_promotion_hint;
#else
    return false;
#endif
  }

  static const gfx::ColorSpace& color_space(
      const cc::TransferableResource& resource) {
    return resource.color_space;
  }

  static bool Read(cc::mojom::TransferableResourceDataView data,
                   cc::TransferableResource* out);
};

}  // namespace mojo

#endif  // CC_IPC_TRANSFERABLE_RESOURCE_STRUCT_TRAITS_H_
