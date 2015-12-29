// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shared_memory.h"

#include <stddef.h>
#include <utility>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "components/exo/buffer.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace exo {
namespace {

bool IsSupportedFormat(gfx::BufferFormat format) {
  return format == gfx::BufferFormat::RGBX_8888 ||
         format == gfx::BufferFormat::RGBA_8888 ||
         format == gfx::BufferFormat::BGRX_8888 ||
         format == gfx::BufferFormat::BGRA_8888;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SharedMemory, public:

SharedMemory::SharedMemory(const base::SharedMemoryHandle& handle)
    : shared_memory_(handle, true /* read-only */) {}

SharedMemory::~SharedMemory() {}

scoped_ptr<Buffer> SharedMemory::CreateBuffer(const gfx::Size& size,
                                              gfx::BufferFormat format,
                                              unsigned offset,
                                              int stride) {
  TRACE_EVENT2("exo", "SharedMemory::CreateBuffer", "size", size.ToString(),
               "format", static_cast<int>(format));

  if (!IsSupportedFormat(format)) {
    DLOG(WARNING) << "Failed to create shm buffer. Unsupported format 0x"
                  << static_cast<int>(format);
    return nullptr;
  }

  if (!base::IsValueInRangeForNumericType<size_t>(stride) ||
      gfx::RowSizeForBufferFormat(size.width(), format, 0) >
          static_cast<size_t>(stride) ||
      static_cast<size_t>(stride) & 3) {
    DLOG(WARNING) << "Failed to create shm buffer. Unsupported stride "
                  << stride;
    return nullptr;
  }

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.handle = base::SharedMemory::DuplicateHandle(shared_memory_.handle());
  handle.offset = offset;
  handle.stride = stride;

  scoped_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      aura::Env::GetInstance()
          ->context_factory()
          ->GetGpuMemoryBufferManager()
          ->CreateGpuMemoryBufferFromHandle(handle, size, format);
  if (!gpu_memory_buffer) {
    LOG(ERROR) << "Failed to create GpuMemoryBuffer from handle";
    return nullptr;
  }

  return make_scoped_ptr(
      new Buffer(std::move(gpu_memory_buffer), GL_TEXTURE_2D));
}

}  // namespace exo
