// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/platform_handle.h"

#include "base/memory/platform_shared_memory_region.h"
#include "build/build_config.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include <mach/mach.h>
#include "base/mac/mach_logging.h"
#endif

namespace mojo {

namespace {

uint64_t PlatformHandleValueFromPlatformFile(base::PlatformFile file) {
#if defined(OS_WIN)
  return reinterpret_cast<uint64_t>(file);
#else
  return static_cast<uint64_t>(file);
#endif
}

base::PlatformFile PlatformFileFromPlatformHandleValue(uint64_t value) {
#if defined(OS_WIN)
  return reinterpret_cast<base::PlatformFile>(value);
#else
  return static_cast<base::PlatformFile>(value);
#endif
}

ScopedSharedBufferHandle WrapPlatformSharedMemoryRegion(
    base::subtle::PlatformSharedMemoryRegion region) {
  if (!region.IsValid())
    return ScopedSharedBufferHandle();

  // TODO(https://crbug.com/826213): Support writable regions too, then simplify
  // mojom base shared memory traits using this code. This will require the C
  // API to be extended first.
  DCHECK_NE(base::subtle::PlatformSharedMemoryRegion::Mode::kWritable,
            region.GetMode());

  base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle handle =
      region.PassPlatformHandle();
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
  const auto& guid = region.GetGUID();
  MojoSharedBufferGuid mojo_guid = {guid.GetHighForSerialization(),
                                    guid.GetLowForSerialization()};
  MojoHandle mojo_handle;
  MojoResult result = MojoWrapPlatformSharedBufferHandle(
      &platform_handle, region.GetSize(), &mojo_guid,
      region.GetMode() ==
              base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly
          ? MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_HANDLE_IS_READ_ONLY
          : MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_NONE,
      &mojo_handle);
  if (result != MOJO_RESULT_OK)
    return ScopedSharedBufferHandle();
  return ScopedSharedBufferHandle(SharedBufferHandle(mojo_handle));
}

base::subtle::PlatformSharedMemoryRegion UnwrapPlatformSharedMemoryRegion(
    ScopedSharedBufferHandle mojo_handle) {
  if (!mojo_handle.is_valid())
    return base::subtle::PlatformSharedMemoryRegion();

  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(platform_handle);
  size_t size;
  MojoSharedBufferGuid mojo_guid;
  MojoPlatformSharedBufferHandleFlags flags;
  MojoResult result = MojoUnwrapPlatformSharedBufferHandle(
      mojo_handle.release().value(), &platform_handle, &size, &mojo_guid,
      &flags);
  if (result != MOJO_RESULT_OK)
    return base::subtle::PlatformSharedMemoryRegion();

  base::subtle::PlatformSharedMemoryRegion::ScopedPlatformHandle region_handle;
#if defined(OS_WIN)
  if (platform_handle.type != MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE)
    return base::subtle::PlatformSharedMemoryRegion();
  region_handle.Set(reinterpret_cast<HANDLE>(platform_handle.value));
#elif defined(OS_FUCHSIA)
  if (platform_handle.type != MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE)
    return base::subtle::PlatformSharedMemoryRegion();
  region_handle.reset(static_cast<zx_handle_t>(platform_handle.value));
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  if (platform_handle.type != MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT)
    return base::subtle::PlatformSharedMemoryRegion();
  region_handle.reset(static_cast<mach_port_t>(platform_handle.value));
#elif defined(OS_ANDROID)
  if (platform_handle.type != MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR)
    return base::subtle::PlatformSharedMemoryRegion();
  region_handle.reset(static_cast<int>(platform_handle.value));
#else
  if (platform_handle.type != MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR)
    return base::subtle::PlatformSharedMemoryRegion();
  region_handle.fd.reset(static_cast<int>(platform_handle.value));
#endif

  // TODO(https://crbug.com/826213): Support unwrapping writable regions. See
  // comment in |WrapPlatformSharedMemoryRegion()| above.
  base::subtle::PlatformSharedMemoryRegion::Mode mode =
      flags & MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_HANDLE_IS_READ_ONLY
          ? base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly
          : base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe;
  return base::subtle::PlatformSharedMemoryRegion::Take(
      std::move(region_handle), mode, size,
      base::UnguessableToken::Deserialize(mojo_guid.high, mojo_guid.low));
}

}  // namespace

ScopedHandle WrapPlatformFile(base::PlatformFile platform_file) {
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(MojoPlatformHandle);
  platform_handle.type = kPlatformFileHandleType;
  platform_handle.value = PlatformHandleValueFromPlatformFile(platform_file);

  MojoHandle mojo_handle;
  MojoResult result = MojoWrapPlatformHandle(&platform_handle, &mojo_handle);
  CHECK_EQ(result, MOJO_RESULT_OK);

  return ScopedHandle(Handle(mojo_handle));
}

MojoResult UnwrapPlatformFile(ScopedHandle handle, base::PlatformFile* file) {
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(MojoPlatformHandle);
  MojoResult result =
      MojoUnwrapPlatformHandle(handle.release().value(), &platform_handle);
  if (result != MOJO_RESULT_OK)
    return result;

  if (platform_handle.type == MOJO_PLATFORM_HANDLE_TYPE_INVALID) {
    *file = base::kInvalidPlatformFile;
  } else {
    CHECK_EQ(platform_handle.type, kPlatformFileHandleType);
    *file = PlatformFileFromPlatformHandleValue(platform_handle.value);
  }

  return MOJO_RESULT_OK;
}

ScopedSharedBufferHandle WrapSharedMemoryHandle(
    const base::SharedMemoryHandle& memory_handle,
    size_t size,
    UnwrappedSharedMemoryHandleProtection protection) {
  if (!memory_handle.IsValid())
    return ScopedSharedBufferHandle();
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(MojoPlatformHandle);
  platform_handle.type = kPlatformSharedBufferHandleType;
#if defined(OS_MACOSX) && !defined(OS_IOS)
  platform_handle.value =
      static_cast<uint64_t>(memory_handle.GetMemoryObject());
#else
  platform_handle.value =
      PlatformHandleValueFromPlatformFile(memory_handle.GetHandle());
#endif

  MojoPlatformSharedBufferHandleFlags flags =
      MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_NONE;
  if (protection == UnwrappedSharedMemoryHandleProtection::kReadOnly) {
    flags |= MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_HANDLE_IS_READ_ONLY;

#if defined(OS_ANDROID)
    // Many callers assume that base::SharedMemory::GetReadOnlyHandle() gives
    // them a handle which is actually read-only. This assumption is invalid on
    // Android. As a precursor to migrating all base::SharedMemory usage --
    // including Mojo internals -- to the new base shared memory API, we ensure
    // that regions are set to read-only if any of their handles are wrapped
    // read-only. This relies on existing usages not attempting to map the
    // region writable any time after this call.
    if (!memory_handle.IsRegionReadOnly())
      memory_handle.SetRegionReadOnly();
#endif
  }

  MojoSharedBufferGuid guid;
  guid.high = memory_handle.GetGUID().GetHighForSerialization();
  guid.low = memory_handle.GetGUID().GetLowForSerialization();
  MojoHandle mojo_handle;
  MojoResult result = MojoWrapPlatformSharedBufferHandle(
      &platform_handle, size, &guid, flags, &mojo_handle);
  CHECK_EQ(result, MOJO_RESULT_OK);

  return ScopedSharedBufferHandle(SharedBufferHandle(mojo_handle));
}

MojoResult UnwrapSharedMemoryHandle(
    ScopedSharedBufferHandle handle,
    base::SharedMemoryHandle* memory_handle,
    size_t* size,
    UnwrappedSharedMemoryHandleProtection* protection) {
  if (!handle.is_valid())
    return MOJO_RESULT_INVALID_ARGUMENT;
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(MojoPlatformHandle);

  MojoPlatformSharedBufferHandleFlags flags;
  size_t num_bytes;
  MojoSharedBufferGuid mojo_guid;
  MojoResult result = MojoUnwrapPlatformSharedBufferHandle(
      handle.release().value(), &platform_handle, &num_bytes, &mojo_guid,
      &flags);
  if (result != MOJO_RESULT_OK)
    return result;

  if (size)
    *size = num_bytes;

  if (protection) {
    *protection =
        flags & MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_HANDLE_IS_READ_ONLY
            ? UnwrappedSharedMemoryHandleProtection::kReadOnly
            : UnwrappedSharedMemoryHandleProtection::kReadWrite;
  }

  base::UnguessableToken guid =
      base::UnguessableToken::Deserialize(mojo_guid.high, mojo_guid.low);
#if defined(OS_MACOSX) && !defined(OS_IOS)
  DCHECK_EQ(platform_handle.type, MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT);
  *memory_handle = base::SharedMemoryHandle(
      static_cast<mach_port_t>(platform_handle.value), num_bytes, guid);
#elif defined(OS_FUCHSIA)
  DCHECK_EQ(platform_handle.type, MOJO_PLATFORM_HANDLE_TYPE_FUCHSIA_HANDLE);
  *memory_handle = base::SharedMemoryHandle(
      static_cast<zx_handle_t>(platform_handle.value), num_bytes, guid);
#elif defined(OS_POSIX)
  DCHECK_EQ(platform_handle.type, MOJO_PLATFORM_HANDLE_TYPE_FILE_DESCRIPTOR);
  *memory_handle = base::SharedMemoryHandle(
      base::FileDescriptor(static_cast<int>(platform_handle.value), false),
      num_bytes, guid);
#elif defined(OS_WIN)
  DCHECK_EQ(platform_handle.type, MOJO_PLATFORM_HANDLE_TYPE_WINDOWS_HANDLE);
  *memory_handle = base::SharedMemoryHandle(
      reinterpret_cast<HANDLE>(platform_handle.value), num_bytes, guid);
#endif

  return MOJO_RESULT_OK;
}

ScopedSharedBufferHandle WrapReadOnlySharedMemoryRegion(
    base::ReadOnlySharedMemoryRegion region) {
  return WrapPlatformSharedMemoryRegion(
      base::ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)));
}

ScopedSharedBufferHandle WrapUnsafeSharedMemoryRegion(
    base::UnsafeSharedMemoryRegion region) {
  return WrapPlatformSharedMemoryRegion(
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(region)));
}

ScopedSharedBufferHandle WrapWritableSharedMemoryRegion(
    base::WritableSharedMemoryRegion region) {
  // TODO(https://crbug.com/826213): Support wrapping writable regions. See
  // comment in |WrapPlatformSharedMemoryRegion()| above.
  NOTIMPLEMENTED();
  return ScopedSharedBufferHandle();
}

base::ReadOnlySharedMemoryRegion UnwrapReadOnlySharedMemoryRegion(
    ScopedSharedBufferHandle handle) {
  return base::ReadOnlySharedMemoryRegion::Deserialize(
      UnwrapPlatformSharedMemoryRegion(std::move(handle)));
}

base::UnsafeSharedMemoryRegion UnwrapUnsafeSharedMemoryRegion(
    ScopedSharedBufferHandle handle) {
  return base::UnsafeSharedMemoryRegion::Deserialize(
      UnwrapPlatformSharedMemoryRegion(std::move(handle)));
}

base::WritableSharedMemoryRegion UnwrapWritableSharedMemoryRegion(
    ScopedSharedBufferHandle handle) {
  // TODO(https://crbug.com/826213): Support unwrapping writable regions. See
  // comment in |WrapPlatformSharedMemoryRegion()| above.
  NOTIMPLEMENTED();
  return base::WritableSharedMemoryRegion();
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
ScopedHandle WrapMachPort(mach_port_t port) {
  kern_return_t kr =
      mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, 1);
  MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
      << "MachPortAttachmentMac mach_port_mod_refs";
  if (kr != KERN_SUCCESS)
    return ScopedHandle();

  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(MojoPlatformHandle);
  platform_handle.type = MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT;
  platform_handle.value = static_cast<uint64_t>(port);

  MojoHandle mojo_handle;
  MojoResult result = MojoWrapPlatformHandle(&platform_handle, &mojo_handle);
  CHECK_EQ(result, MOJO_RESULT_OK);

  return ScopedHandle(Handle(mojo_handle));
}

MojoResult UnwrapMachPort(ScopedHandle handle, mach_port_t* port) {
  MojoPlatformHandle platform_handle;
  platform_handle.struct_size = sizeof(MojoPlatformHandle);
  MojoResult result =
      MojoUnwrapPlatformHandle(handle.release().value(), &platform_handle);
  if (result != MOJO_RESULT_OK)
    return result;

  CHECK(platform_handle.type == MOJO_PLATFORM_HANDLE_TYPE_MACH_PORT ||
        platform_handle.type == MOJO_PLATFORM_HANDLE_TYPE_INVALID);
  *port = static_cast<mach_port_t>(platform_handle.value);
  return MOJO_RESULT_OK;
}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

}  // namespace mojo
