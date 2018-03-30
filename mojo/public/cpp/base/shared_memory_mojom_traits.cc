// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/shared_memory_mojom_traits.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "mojo/public/c/system/platform_handle.h"

namespace mojo {

// static
mojo::ScopedHandle
StructTraits<mojo_base::mojom::PlatformSharedMemoryHandleDataView,
             base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle>::
    handle_value(base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle&
                     handle) {
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(platform_handle);
#if defined(OS_WIN)
  platform_handle.type = MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE;
  platform_handle.value = reinterpret_cast<uint64_t>(handle.Take());
#elif defined(OS_FUCHSIA)
  platform_handle.type = MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE;
  platform_handle.value = static_cast<uint64_t>(handle.release());
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  platform_handle.type = MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT;
  platform_handle.value = static_cast<uint64_t>(handle.release());
#elif defined(OS_ANDROID)
  platform_handle.type = MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
  platform_handle.value = static_cast<uint64_t>(handle.release());
#else
  platform_handle.type = MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
  platform_handle.value = static_cast<uint64_t>(handle.fd.release());
#endif
  MojoHandle mojo_handle;
  MojoResult result = MojoWrapPlatformHandle(&platform_handle, &mojo_handle);
  if (result != MOJO_RESULT_OK)
    return mojo::ScopedHandle();
  return mojo::ScopedHandle(mojo::Handle(mojo_handle));
}

#if defined(OS_POSIX) && !defined(OS_ANDROID) && !defined(OS_FUCHSIA) && \
    (!defined(OS_MACOSX) || defined(OS_IOS))
// static
mojo::ScopedHandle
StructTraits<mojo_base::mojom::PlatformSharedMemoryHandleDataView,
             base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle>::
    readonly_fd(base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle&
                    handle) {
  if (!handle.readonly_fd.is_valid())
    return mojo::ScopedHandle();
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(platform_handle);
  platform_handle.type = MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR;
  platform_handle.value = static_cast<uint64_t>(handle.readonly_fd.release());

  MojoHandle mojo_handle;
  MojoResult result = MojoWrapPlatformHandle(&platform_handle, &mojo_handle);
  if (result != MOJO_RESULT_OK)
    return mojo::ScopedHandle();

  return mojo::ScopedHandle(mojo::Handle(mojo_handle));
}
#endif

bool StructTraits<
    mojo_base::mojom::PlatformSharedMemoryHandleDataView,
    base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle>::
    Read(mojo_base::mojom::PlatformSharedMemoryHandleDataView data,
         base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle* out) {
  mojo::ScopedHandle mojo_handle = data.TakeHandleValue();
  if (!mojo_handle.is_valid())
    return false;

  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(platform_handle);
  MojoResult result =
      MojoUnwrapPlatformHandle(mojo_handle.release().value(), &platform_handle);
  if (result != MOJO_RESULT_OK)
    return false;

#if defined(OS_WIN)
  if (platform_handle.type != MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE)
    return false;
  out->Set(reinterpret_cast<HANDLE>(platform_handle.value));
#elif defined(OS_FUCHSIA)
  if (platform_handle.type != MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE)
    return false;
  out->reset(static_cast<zx_handle_t>(platform_handle.value));
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  if (platform_handle.type != MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT)
    return false;
  out->reset(static_cast<mach_port_t>(platform_handle.value));
#elif defined(OS_ANDROID)
  if (platform_handle.type != MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR)
    return false;
  out->reset(static_cast<int>(platform_handle.value));
#else
  if (platform_handle.type != MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR)
    return false;
  out->fd.reset(static_cast<int>(platform_handle.value));

  // Other POSIX platforms may have an additional FD to support eventual
  // read-only duplication.
  mojo::ScopedHandle readonly_mojo_handle = data.TakeReadonlyFd();

  MojoPlatformHandle readonly_platform_handle;
  readonly_platform_handle.struct_size = sizeof(readonly_platform_handle);
  readonly_platform_handle.type = MOJO_PLATFORM_HANDLE_TYPE_INVALID;
  if (readonly_mojo_handle.is_valid()) {
    MojoResult result = MojoUnwrapPlatformHandle(
        readonly_mojo_handle.release().value(), &readonly_platform_handle);
    if (result != MOJO_RESULT_OK ||
        readonly_platform_handle.type !=
            MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR) {
      return false;
    }
    out->readonly_fd.reset(static_cast<int>(readonly_platform_handle.value));
  }
#endif

  return true;
}

// static
mojo_base::mojom::PlatformSharedMemoryRegion_Mode
EnumTraits<mojo_base::mojom::PlatformSharedMemoryRegion_Mode,
           base::subtle::PlatformSharedMemoryRegion::Mode>::
    ToMojom(base::subtle::PlatformSharedMemoryRegion::Mode input) {
  switch (input) {
    case base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly:
      return mojo_base::mojom::PlatformSharedMemoryRegion_Mode::kReadOnly;
    case base::subtle::PlatformSharedMemoryRegion::Mode::kWritable:
      return mojo_base::mojom::PlatformSharedMemoryRegion_Mode::kWritable;
    case base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe:
      return mojo_base::mojom::PlatformSharedMemoryRegion_Mode::kUnsafe;
    default:
      NOTREACHED();
      return mojo_base::mojom::PlatformSharedMemoryRegion_Mode::kUnsafe;
  }
}

// static
bool EnumTraits<mojo_base::mojom::PlatformSharedMemoryRegion_Mode,
                base::subtle::PlatformSharedMemoryRegion::Mode>::
    FromMojom(mojo_base::mojom::PlatformSharedMemoryRegion_Mode input,
              base::subtle::PlatformSharedMemoryRegion::Mode* output) {
  switch (input) {
    case mojo_base::mojom::PlatformSharedMemoryRegion_Mode::kReadOnly:
      *output = base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly;
      break;
    case mojo_base::mojom::PlatformSharedMemoryRegion_Mode::kWritable:
      *output = base::subtle::PlatformSharedMemoryRegion::Mode::kWritable;
      break;
    case mojo_base::mojom::PlatformSharedMemoryRegion_Mode::kUnsafe:
      *output = base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe;
      break;
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

// static
base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle
StructTraits<mojo_base::mojom::PlatformSharedMemoryRegionDataView,
             base::subtle::PlatformSharedMemoryRegion>::
    platform_handle(base::subtle::PlatformSharedMemoryRegion& region) {
  return region.PassPlatformHandle();
}

// static
base::subtle::PlatformSharedMemoryRegion::Mode
StructTraits<mojo_base::mojom::PlatformSharedMemoryRegionDataView,
             base::subtle::PlatformSharedMemoryRegion>::
    mode(const base::subtle::PlatformSharedMemoryRegion& region) {
  return region.GetMode();
}

// static
uint64_t StructTraits<mojo_base::mojom::PlatformSharedMemoryRegionDataView,
                      base::subtle::PlatformSharedMemoryRegion>::
    size(const base::subtle::PlatformSharedMemoryRegion& region) {
  return region.GetSize();
}

// static
base::UnguessableToken
StructTraits<mojo_base::mojom::PlatformSharedMemoryRegionDataView,
             base::subtle::PlatformSharedMemoryRegion>::
    guid(const base::subtle::PlatformSharedMemoryRegion& region) {
  return region.GetGUID();
}

// static
bool StructTraits<mojo_base::mojom::PlatformSharedMemoryRegionDataView,
                  base::subtle::PlatformSharedMemoryRegion>::
    Read(mojo_base::mojom::PlatformSharedMemoryRegionDataView data,
         base::subtle::PlatformSharedMemoryRegion* out) {
  if (!base::IsValueInRangeForNumericType<size_t>(data.size()))
    return false;
  size_t size = static_cast<size_t>(data.size());
  base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle handle;
  base::subtle::PlatformSharedMemoryRegion::Mode mode;
  base::UnguessableToken guid;
  if (!data.ReadPlatformHandle(&handle) || !data.ReadMode(&mode) ||
      !data.ReadGuid(&guid)) {
    return false;
  }
  *out = base::subtle::PlatformSharedMemoryRegion::Take(std::move(handle), mode,
                                                        size, guid);
  return out->IsValid();
}

// static
bool StructTraits<mojo_base::mojom::ReadOnlySharedMemoryRegionDataView,
                  base::ReadOnlySharedMemoryRegion>::
    IsNull(const base::ReadOnlySharedMemoryRegion& region) {
  return !region.IsValid();
}

// static
void StructTraits<mojo_base::mojom::ReadOnlySharedMemoryRegionDataView,
                  base::ReadOnlySharedMemoryRegion>::
    SetToNull(base::ReadOnlySharedMemoryRegion* region) {
  *region = base::ReadOnlySharedMemoryRegion();
}

// static
base::subtle::PlatformSharedMemoryRegion StructTraits<
    mojo_base::mojom::ReadOnlySharedMemoryRegionDataView,
    base::ReadOnlySharedMemoryRegion>::region(base::ReadOnlySharedMemoryRegion&
                                                  in_region) {
  return base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
      std::move(in_region));
}

// static
bool StructTraits<mojo_base::mojom::ReadOnlySharedMemoryRegionDataView,
                  base::ReadOnlySharedMemoryRegion>::
    Read(mojo_base::mojom::ReadOnlySharedMemoryRegionDataView data,
         base::ReadOnlySharedMemoryRegion* out) {
  base::subtle::PlatformSharedMemoryRegion region;
  if (!data.ReadRegion(&region))
    return false;
  *out = base::ReadOnlySharedMemoryRegion::Deserialize(std::move(region));
  return out->IsValid();
}

// static
bool StructTraits<mojo_base::mojom::UnsafeSharedMemoryRegionDataView,
                  base::UnsafeSharedMemoryRegion>::
    IsNull(const base::UnsafeSharedMemoryRegion& region) {
  return !region.IsValid();
}

// static
void StructTraits<mojo_base::mojom::UnsafeSharedMemoryRegionDataView,
                  base::UnsafeSharedMemoryRegion>::
    SetToNull(base::UnsafeSharedMemoryRegion* region) {
  *region = base::UnsafeSharedMemoryRegion();
}

// static
base::subtle::PlatformSharedMemoryRegion StructTraits<
    mojo_base::mojom::UnsafeSharedMemoryRegionDataView,
    base::UnsafeSharedMemoryRegion>::region(base::UnsafeSharedMemoryRegion&
                                                in_region) {
  return base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
      std::move(in_region));
}

// static
bool StructTraits<mojo_base::mojom::UnsafeSharedMemoryRegionDataView,
                  base::UnsafeSharedMemoryRegion>::
    Read(mojo_base::mojom::UnsafeSharedMemoryRegionDataView data,
         base::UnsafeSharedMemoryRegion* out) {
  base::subtle::PlatformSharedMemoryRegion region;
  if (!data.ReadRegion(&region))
    return false;
  *out = base::UnsafeSharedMemoryRegion::Deserialize(std::move(region));
  return out->IsValid();
}

// static
bool StructTraits<mojo_base::mojom::WritableSharedMemoryRegionDataView,
                  base::WritableSharedMemoryRegion>::
    IsNull(const base::WritableSharedMemoryRegion& region) {
  return !region.IsValid();
}

// static
void StructTraits<mojo_base::mojom::WritableSharedMemoryRegionDataView,
                  base::WritableSharedMemoryRegion>::
    SetToNull(base::WritableSharedMemoryRegion* region) {
  *region = base::WritableSharedMemoryRegion();
}

// static
base::subtle::PlatformSharedMemoryRegion StructTraits<
    mojo_base::mojom::WritableSharedMemoryRegionDataView,
    base::WritableSharedMemoryRegion>::region(base::WritableSharedMemoryRegion&
                                                  in_region) {
  return base::WritableSharedMemoryRegion::TakeHandleForSerialization(
      std::move(in_region));
}

// static
bool StructTraits<mojo_base::mojom::WritableSharedMemoryRegionDataView,
                  base::WritableSharedMemoryRegion>::
    Read(mojo_base::mojom::WritableSharedMemoryRegionDataView data,
         base::WritableSharedMemoryRegion* out) {
  base::subtle::PlatformSharedMemoryRegion region;
  if (!data.ReadRegion(&region))
    return false;
  *out = base::WritableSharedMemoryRegion::Deserialize(std::move(region));
  return out->IsValid();
}

}  // namespace mojo
