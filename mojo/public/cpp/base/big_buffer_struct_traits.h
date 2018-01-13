// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_BIG_BUFFER_STRUCT_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_BIG_BUFFER_STRUCT_TRAITS_H_

#include <cstdint>
#include <vector>

#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/mojo_base_traits_export.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/mojom/base/big_buffer.mojom-shared.h"

namespace mojo {

template <>
struct MOJO_BASE_TRAITS_EXPORT
    StructTraits<mojo_base::mojom::BigBufferSharedMemoryRegionDataView,
                 mojo_base::internal::BigBufferSharedMemoryRegion> {
  static uint32_t size(
      const mojo_base::internal::BigBufferSharedMemoryRegion& region);
  static mojo::ScopedSharedBufferHandle buffer_handle(
      mojo_base::internal::BigBufferSharedMemoryRegion& region);

  static bool Read(mojo_base::mojom::BigBufferSharedMemoryRegionDataView data,
                   mojo_base::internal::BigBufferSharedMemoryRegion* out);
};

template <>
struct MOJO_BASE_TRAITS_EXPORT
    UnionTraits<mojo_base::mojom::BigBufferDataView, mojo_base::BigBuffer> {
  static mojo_base::mojom::BigBufferDataView::Tag GetTag(
      const mojo_base::BigBuffer& buffer);

  static const std::vector<uint8_t>& bytes(const mojo_base::BigBuffer& buffer);
  static mojo_base::internal::BigBufferSharedMemoryRegion& shared_memory(
      mojo_base::BigBuffer& buffer);

  static bool Read(mojo_base::mojom::BigBufferDataView data,
                   mojo_base::BigBuffer* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_BIG_BUFFER_STRUCT_TRAITS_H_
