// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_METAL_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_COMMON_GPU_METAL_CONTEXT_PROVIDER_H_

#include <memory>
#include "components/viz/common/viz_metal_context_provider_export.h"

class GrContext;

#if __OBJC__
@protocol MTLDevice;
using MTLDevicePtr = id<MTLDevice>;
#else
class MTLDeviceProtocol;
using MTLDevicePtr = MTLDeviceProtocol*;
#endif

namespace viz {

// The MetalContextProvider provides a Metal-backed GrContext.
class VIZ_METAL_CONTEXT_PROVIDER_EXPORT MetalContextProvider {
 public:
  // Create and return a MetalContextProvider if possible. May return nullptr
  // if no Metal devices exist.
  static std::unique_ptr<MetalContextProvider> Create();
  virtual ~MetalContextProvider() {}

  virtual GrContext* GetGrContext() = 0;
  virtual MTLDevicePtr GetMTLDevice() = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_METAL_CONTEXT_PROVIDER_H_
