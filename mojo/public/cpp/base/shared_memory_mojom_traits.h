// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_SHARED_MEMORY_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_SHARED_MEMORY_MOJOM_TRAITS_H_

#include <cstdint>

#include "base/component_export.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/mojom/base/shared_memory.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_MOJOM) StructTraits<
    mojo_base::mojom::PlatformSharedMemoryHandleDataView,
    base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle> {
  static mojo::ScopedHandle handle_value(
      base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle& handle);
#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_FUCHSIA) && \
    (!defined(OS_MACOSX) || defined(OS_IOS))
  static mojo::ScopedHandle readonly_fd(
      base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle& handle);
#endif

  static bool Read(
      mojo_base::mojom::PlatformSharedMemoryHandleDataView data,
      base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle* out);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_MOJOM)
    EnumTraits<mojo_base::mojom::PlatformSharedMemoryRegion_Mode,
               base::subtle::PlatformSharedMemoryRegion::Mode> {
  static mojo_base::mojom::PlatformSharedMemoryRegion_Mode ToMojom(
      base::subtle::PlatformSharedMemoryRegion::Mode input);

  static bool FromMojom(mojo_base::mojom::PlatformSharedMemoryRegion_Mode input,
                        base::subtle::PlatformSharedMemoryRegion::Mode* output);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_MOJOM)
    StructTraits<mojo_base::mojom::PlatformSharedMemoryRegionDataView,
                 base::subtle::PlatformSharedMemoryRegion> {
  static base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle
  platform_handle(base::subtle::PlatformSharedMemoryRegion& region);

  static base::subtle::PlatformSharedMemoryRegion::Mode mode(
      const base::subtle::PlatformSharedMemoryRegion& region);

  static uint64_t size(const base::subtle::PlatformSharedMemoryRegion& region);

  static base::UnguessableToken guid(
      const base::subtle::PlatformSharedMemoryRegion& region);

  static bool Read(mojo_base::mojom::PlatformSharedMemoryRegionDataView data,
                   base::subtle::PlatformSharedMemoryRegion* out);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_MOJOM)
    StructTraits<mojo_base::mojom::ReadOnlySharedMemoryRegionDataView,
                 base::ReadOnlySharedMemoryRegion> {
  static bool IsNull(const base::ReadOnlySharedMemoryRegion& region);
  static void SetToNull(base::ReadOnlySharedMemoryRegion* region);
  static base::subtle::PlatformSharedMemoryRegion region(
      base::ReadOnlySharedMemoryRegion& in_region);
  static bool Read(mojo_base::mojom::ReadOnlySharedMemoryRegionDataView data,
                   base::ReadOnlySharedMemoryRegion* out);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_MOJOM)
    StructTraits<mojo_base::mojom::UnsafeSharedMemoryRegionDataView,
                 base::UnsafeSharedMemoryRegion> {
  static bool IsNull(const base::UnsafeSharedMemoryRegion& region);
  static void SetToNull(base::UnsafeSharedMemoryRegion* region);
  static base::subtle::PlatformSharedMemoryRegion region(
      base::UnsafeSharedMemoryRegion& in_region);
  static bool Read(mojo_base::mojom::UnsafeSharedMemoryRegionDataView data,
                   base::UnsafeSharedMemoryRegion* out);
};

template <>
struct COMPONENT_EXPORT(MOJO_BASE_MOJOM)
    StructTraits<mojo_base::mojom::WritableSharedMemoryRegionDataView,
                 base::WritableSharedMemoryRegion> {
  static bool IsNull(const base::WritableSharedMemoryRegion& region);
  static void SetToNull(base::WritableSharedMemoryRegion* region);
  static base::subtle::PlatformSharedMemoryRegion region(
      base::WritableSharedMemoryRegion& in_region);
  static bool Read(mojo_base::mojom::WritableSharedMemoryRegionDataView data,
                   base::WritableSharedMemoryRegion* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_SHARED_MEMORY_MOJOM_TRAITS_H_
