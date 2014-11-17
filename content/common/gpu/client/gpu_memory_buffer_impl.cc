// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "ui/gl/gl_bindings.h"

namespace content {
namespace {

gfx::GpuMemoryBufferType g_preferred_type = gfx::EMPTY_BUFFER;

struct DefaultPreferredType {
  DefaultPreferredType() : value(gfx::EMPTY_BUFFER) {
    std::vector<gfx::GpuMemoryBufferType> supported_types;
    GpuMemoryBufferImpl::GetSupportedTypes(&supported_types);
    DCHECK(!supported_types.empty());
    value = supported_types[0];
  }
  gfx::GpuMemoryBufferType value;
};
base::LazyInstance<DefaultPreferredType> g_default_preferred_type =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

GpuMemoryBufferImpl::GpuMemoryBufferImpl(gfx::GpuMemoryBufferId id,
                                         const gfx::Size& size,
                                         Format format,
                                         const DestructionCallback& callback)
    : id_(id),
      size_(size),
      format_(format),
      callback_(callback),
      mapped_(false),
      destruction_sync_point_(0) {
}

GpuMemoryBufferImpl::~GpuMemoryBufferImpl() {
  callback_.Run(destruction_sync_point_);
}

// static
void GpuMemoryBufferImpl::SetPreferredType(gfx::GpuMemoryBufferType type) {
  // EMPTY_BUFFER is a reserved value and not a valid preferred type.
  DCHECK_NE(gfx::EMPTY_BUFFER, type);

  // Make sure this function is only called once before the first call
  // to GetPreferredType().
  DCHECK_EQ(gfx::EMPTY_BUFFER, g_preferred_type);

  g_preferred_type = type;
}

// static
gfx::GpuMemoryBufferType GpuMemoryBufferImpl::GetPreferredType() {
  if (g_preferred_type == gfx::EMPTY_BUFFER)
    g_preferred_type = g_default_preferred_type.Get().value;

  return g_preferred_type;
}

// static
GpuMemoryBufferImpl* GpuMemoryBufferImpl::FromClientBuffer(
    ClientBuffer buffer) {
  return reinterpret_cast<GpuMemoryBufferImpl*>(buffer);
}

// static
size_t GpuMemoryBufferImpl::BytesPerPixel(Format format) {
  switch (format) {
    case RGBA_8888:
    case RGBX_8888:
    case BGRA_8888:
      return 4;
  }

  NOTREACHED();
  return 0;
}

gfx::GpuMemoryBuffer::Format GpuMemoryBufferImpl::GetFormat() const {
  return format_;
}

bool GpuMemoryBufferImpl::IsMapped() const {
  return mapped_;
}

ClientBuffer GpuMemoryBufferImpl::AsClientBuffer() {
  return reinterpret_cast<ClientBuffer>(this);
}

}  // namespace content
