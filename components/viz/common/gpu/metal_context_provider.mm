// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/metal_context_provider.h"

#include "base/mac/scoped_nsobject.h"
#include "third_party/skia/include/gpu/GrContext.h"

#import <Metal/Metal.h>

namespace viz {

namespace {

struct API_AVAILABLE(macos(10.11)) MetalContextProviderImpl
    : public MetalContextProvider {
 public:
  explicit MetalContextProviderImpl(
      base::scoped_nsprotocol<id<MTLDevice>> device)
      : device_(device) {
    command_queue_.reset([device_ newCommandQueue]);
    // GrContext::MakeMetal will take ownership of the objects passed in. Retain
    // the objects before passing them to MakeMetal so that the objects in
    // |this| are also valid.
    gr_context_ =
        GrContext::MakeMetal([device_ retain], [command_queue_ retain]);
    DCHECK(gr_context_);
  }
  ~MetalContextProviderImpl() override {}
  GrContext* GetGrContext() override { return gr_context_.get(); }
  MTLDevicePtr GetMTLDevice() override { return device_.get(); }

 private:
  base::scoped_nsprotocol<id<MTLDevice>> device_;
  base::scoped_nsprotocol<id<MTLCommandQueue>> command_queue_;
  sk_sp<GrContext> gr_context_;

  DISALLOW_COPY_AND_ASSIGN(MetalContextProviderImpl);
};

}  // namespace

// static
std::unique_ptr<MetalContextProvider> MetalContextProvider::Create() {
  if (@available(macOS 10.11, *)) {
    // First attempt to find a low power device to use.
    base::scoped_nsprotocol<id<MTLDevice>> device_to_use;
    base::scoped_nsobject<NSArray<id<MTLDevice>>> devices(MTLCopyAllDevices());
    for (id<MTLDevice> device in devices.get()) {
      if ([device isLowPower]) {
        device_to_use.reset(device, base::scoped_policy::RETAIN);
        break;
      }
    }
    // Failing that, use the system default device.
    if (!device_to_use)
      device_to_use.reset(MTLCreateSystemDefaultDevice());
    if (device_to_use)
      return std::make_unique<MetalContextProviderImpl>(device_to_use);
    DLOG(ERROR) << "Failed to create MTLDevice.";
  }
  // If no device was found, or if the macOS version is too old for Metal,
  // return no context provider.
  return nullptr;
}

}  // namespace viz
