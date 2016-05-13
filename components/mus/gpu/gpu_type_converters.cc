// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gpu/gpu_type_converters.h"

#include "build/build_config.h"
#include "ipc/ipc_channel_handle.h"
#include "mojo/platform_handle/platform_handle_functions.h"
#include "ui/gfx/gpu_memory_buffer.h"

#if defined(USE_OZONE)
#include "ui/gfx/native_pixmap_handle_ozone.h"
#endif

namespace mojo {

// static
mus::mojom::ChannelHandlePtr
TypeConverter<mus::mojom::ChannelHandlePtr, IPC::ChannelHandle>::Convert(
    const IPC::ChannelHandle& handle) {
  mus::mojom::ChannelHandlePtr result = mus::mojom::ChannelHandle::New();
  result->name = handle.name;
#if defined(OS_WIN)
  // On windows, a pipe handle Will NOT be marshalled over IPC.
  DCHECK(handle.pipe.handle == NULL);
#else
  DCHECK(handle.socket.auto_close || handle.socket.fd == -1);
  MojoPlatformHandle platform_handle = handle.socket.fd;
  MojoHandle mojo_handle = MOJO_HANDLE_INVALID;
  if (platform_handle != -1) {
    MojoResult create_result =
        MojoCreatePlatformHandleWrapper(platform_handle, &mojo_handle);
    if (create_result == MOJO_RESULT_OK)
      result->socket.reset(mojo::Handle(mojo_handle));
  }
#endif
  return result;
}

// static
IPC::ChannelHandle
TypeConverter<IPC::ChannelHandle, mus::mojom::ChannelHandlePtr>::Convert(
    const mus::mojom::ChannelHandlePtr& handle) {
  if (handle.is_null())
    return IPC::ChannelHandle();
#if defined(OS_WIN)
  // On windows, a pipe handle Will NOT be marshalled over IPC.
  DCHECK(!handle->socket.is_valid());
  return IPC::ChannelHandle(handle->name);
#else
  MojoPlatformHandle platform_handle = -1;
  MojoExtractPlatformHandle(handle->socket.release().value(), &platform_handle);
  return IPC::ChannelHandle(handle->name,
                            base::FileDescriptor(platform_handle, true));
#endif
}

#if defined(USE_OZONE)
// static
mus::mojom::NativePixmapHandlePtr TypeConverter<
    mus::mojom::NativePixmapHandlePtr,
    gfx::NativePixmapHandle>::Convert(const gfx::NativePixmapHandle& handle) {
  // TODO(penghuang); support NativePixmapHandle.
  mus::mojom::NativePixmapHandlePtr result =
      mus::mojom::NativePixmapHandle::New();
  return result;
}

// static
gfx::NativePixmapHandle
TypeConverter<gfx::NativePixmapHandle, mus::mojom::NativePixmapHandlePtr>::
    Convert(const mus::mojom::NativePixmapHandlePtr& handle) {
  // TODO(penghuang); support NativePixmapHandle.
  gfx::NativePixmapHandle result;
  return result;
}
#endif

// static
mus::mojom::GpuMemoryBufferIdPtr TypeConverter<
    mus::mojom::GpuMemoryBufferIdPtr,
    gfx::GpuMemoryBufferId>::Convert(const gfx::GpuMemoryBufferId& id) {
  mus::mojom::GpuMemoryBufferIdPtr result =
      mus::mojom::GpuMemoryBufferId::New();
  result->id = id.id;
  return result;
}

// static
gfx::GpuMemoryBufferId
TypeConverter<gfx::GpuMemoryBufferId, mus::mojom::GpuMemoryBufferIdPtr>::
    Convert(const mus::mojom::GpuMemoryBufferIdPtr& id) {
  return gfx::GpuMemoryBufferId(id->id);
}

// static
mus::mojom::GpuMemoryBufferHandlePtr TypeConverter<
    mus::mojom::GpuMemoryBufferHandlePtr,
    gfx::GpuMemoryBufferHandle>::Convert(const gfx::GpuMemoryBufferHandle&
                                             handle) {
  DCHECK(handle.type == gfx::SHARED_MEMORY_BUFFER);
  mus::mojom::GpuMemoryBufferHandlePtr result =
      mus::mojom::GpuMemoryBufferHandle::New();
  result->type = static_cast<mus::mojom::GpuMemoryBufferType>(handle.type);
  result->id = mus::mojom::GpuMemoryBufferId::From(handle.id);
  MojoPlatformHandle platform_handle;
#if defined(OS_WIN)
  platform_handle = handle.handle.GetHandle();
#else
  DCHECK(handle.handle.auto_close || handle.handle.fd == -1);
  platform_handle = handle.handle.fd;
#endif
  MojoHandle mojo_handle = MOJO_HANDLE_INVALID;
  MojoResult create_result =
      MojoCreatePlatformHandleWrapper(platform_handle, &mojo_handle);
  if (create_result == MOJO_RESULT_OK)
    result->buffer_handle.reset(mojo::Handle(mojo_handle));
  result->offset = handle.offset;
  result->stride = handle.stride;
#if defined(USE_OZONE)
  result->native_pixmap_handle =
      mus::mojom::NativePixmapHandle::From(handle.native_pixmap_handle);
#endif
  return result;
}

// static
gfx::GpuMemoryBufferHandle TypeConverter<gfx::GpuMemoryBufferHandle,
                                         mus::mojom::GpuMemoryBufferHandlePtr>::
    Convert(const mus::mojom::GpuMemoryBufferHandlePtr& handle) {
  DCHECK(handle->type == mus::mojom::GpuMemoryBufferType::SHARED_MEMORY);
  gfx::GpuMemoryBufferHandle result;
  result.type = static_cast<gfx::GpuMemoryBufferType>(handle->type);
  result.id = handle->id.To<gfx::GpuMemoryBufferId>();
  MojoPlatformHandle platform_handle;
  MojoResult extract_result = MojoExtractPlatformHandle(
      handle->buffer_handle.release().value(), &platform_handle);
  if (extract_result == MOJO_RESULT_OK) {
#if defined(OS_WIN)
    result.handle =
        base::SharedMemoryHandle(platform_handle, base::GetCurrentProcId());
#else
    result.handle = base::SharedMemoryHandle(platform_handle, true);
#endif
  }
  result.offset = handle->offset;
  result.stride = handle->stride;
#if defined(USE_OZONE)
  result.native_pixmap_handle =
      handle->native_pixmap_handle.To<gfx::NativePixmapHandle>();
#else
  DCHECK(handle->native_pixmap_handle.is_null());
#endif
  return result;
}

}  // namespace mojo
