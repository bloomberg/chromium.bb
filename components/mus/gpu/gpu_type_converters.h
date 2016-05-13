// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GPU_GPU_TYPE_CONVERTERS_H_
#define COMPONENTS_MUS_GPU_GPU_TYPE_CONVERTERS_H_

#include "build/build_config.h"
#include "components/mus/public/interfaces/channel_handle.mojom.h"
#include "components/mus/public/interfaces/gpu_memory_buffer.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace gfx {
struct GpuMemoryBufferHandle;
class GenericSharedMemoryId;
using GpuMemoryBufferId = GenericSharedMemoryId;
struct NativePixmapHandle;
}

namespace IPC {
struct ChannelHandle;
}

namespace mojo {

template <>
struct TypeConverter<mus::mojom::ChannelHandlePtr, IPC::ChannelHandle> {
  static mus::mojom::ChannelHandlePtr Convert(const IPC::ChannelHandle& handle);
};

template <>
struct TypeConverter<IPC::ChannelHandle, mus::mojom::ChannelHandlePtr> {
  static IPC::ChannelHandle Convert(const mus::mojom::ChannelHandlePtr& handle);
};

#if defined(USE_OZONE)
template <>
struct TypeConverter<mus::mojom::NativePixmapHandlePtr,
                     gfx::NativePixmapHandle> {
  static mus::mojom::NativePixmapHandlePtr Convert(
      const gfx::NativePixmapHandle& handle);
};

template <>
struct TypeConverter<gfx::NativePixmapHandle,
                     mus::mojom::NativePixmapHandlePtr> {
  static gfx::NativePixmapHandle Convert(
      const mus::mojom::NativePixmapHandlePtr& handle);
};
#endif

template <>
struct TypeConverter<mus::mojom::GpuMemoryBufferIdPtr, gfx::GpuMemoryBufferId> {
  static mus::mojom::GpuMemoryBufferIdPtr Convert(
      const gfx::GpuMemoryBufferId& id);
};

template <>
struct TypeConverter<gfx::GpuMemoryBufferId, mus::mojom::GpuMemoryBufferIdPtr> {
  static gfx::GpuMemoryBufferId Convert(
      const mus::mojom::GpuMemoryBufferIdPtr& id);
};

template <>
struct TypeConverter<mus::mojom::GpuMemoryBufferHandlePtr,
                     gfx::GpuMemoryBufferHandle> {
  static mus::mojom::GpuMemoryBufferHandlePtr Convert(
      const gfx::GpuMemoryBufferHandle& handle);
};

template <>
struct TypeConverter<gfx::GpuMemoryBufferHandle,
                     mus::mojom::GpuMemoryBufferHandlePtr> {
  static gfx::GpuMemoryBufferHandle Convert(
      const mus::mojom::GpuMemoryBufferHandlePtr& handle);
};

}  // namespace mojo

#endif  // COMPONENTS_MUS_GPU_GPU_TYPE_CONVERTERS_H_
