// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_TRANSFERABLE_RESOURCE_STRUCT_TRAITS_H_
#define CC_IPC_TRANSFERABLE_RESOURCE_STRUCT_TRAITS_H_

#include "cc/ipc/transferable_resource.mojom.h"
#include "cc/resources/transferable_resource.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::TransferableResource, cc::TransferableResource> {
  static uint32_t id(const cc::TransferableResource& resource) {
    return resource.id;
  }

  static cc::mojom::ResourceFormat format(
      const cc::TransferableResource& resource) {
    return static_cast<cc::mojom::ResourceFormat>(resource.format);
  }

  static uint32_t filter(const cc::TransferableResource& resource) {
    return resource.filter;
  }

  static gfx::Size size(const cc::TransferableResource& resource) {
    return resource.size;
  }

  static gpu::MailboxHolder mailbox_holder(
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

  static int32_t gpu_memory_buffer_id(
      const cc::TransferableResource& resource) {
    return resource.gpu_memory_buffer_id.id;
  }

  static bool is_overlay_candidate(const cc::TransferableResource& resource) {
    return resource.is_overlay_candidate;
  }

  static bool Read(cc::mojom::TransferableResourceDataView data,
                   cc::TransferableResource* out) {
    gfx::Size size;
    gpu::MailboxHolder holder;
    if (!data.ReadSize(&size) || !data.ReadMailboxHolder(&holder))
      return false;
    out->id = data.id();
    out->format = static_cast<cc::ResourceFormat>(data.format());
    out->filter = data.filter();
    out->size = size;
    out->mailbox_holder = holder;
    out->read_lock_fences_enabled = data.read_lock_fences_enabled();
    out->is_software = data.is_software();
    out->gpu_memory_buffer_id.id = data.gpu_memory_buffer_id();
    out->is_overlay_candidate = data.is_overlay_candidate();
    return true;
  }
};

}  // namespace mojo

#endif  // CC_IPC_TRANSFERABLE_RESOURCE_STRUCT_TRAITS_H_
