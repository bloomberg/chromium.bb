// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/platform_handle.h"

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
    bool read_only) {
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
  if (read_only)
    flags |= MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_READ_ONLY;

  MojoSharedBufferGuid guid;
  guid.high = memory_handle.GetGUID().GetHighForSerialization();
  guid.low = memory_handle.GetGUID().GetLowForSerialization();
  MojoHandle mojo_handle;
  MojoResult result = MojoWrapPlatformSharedBufferHandle(
      &platform_handle, size, &guid, flags, &mojo_handle);
  CHECK_EQ(result, MOJO_RESULT_OK);

  return ScopedSharedBufferHandle(SharedBufferHandle(mojo_handle));
}

MojoResult UnwrapSharedMemoryHandle(ScopedSharedBufferHandle handle,
                                    base::SharedMemoryHandle* memory_handle,
                                    size_t* size,
                                    bool* read_only) {
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

  if (read_only)
    *read_only = flags & MOJO_PLATFORM_SHARED_BUFFER_HANDLE_FLAG_READ_ONLY;

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
