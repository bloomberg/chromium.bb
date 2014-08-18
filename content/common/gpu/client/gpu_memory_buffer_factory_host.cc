// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_factory_host.h"

#include "base/logging.h"

namespace content {
namespace {
GpuMemoryBufferFactoryHost* instance = NULL;
}

// static
GpuMemoryBufferFactoryHost* GpuMemoryBufferFactoryHost::GetInstance() {
  return instance;
}

GpuMemoryBufferFactoryHost::GpuMemoryBufferFactoryHost() {
  DCHECK(instance == NULL);
  instance = this;
}

GpuMemoryBufferFactoryHost::~GpuMemoryBufferFactoryHost() {
  instance = NULL;
}
}
