// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/surface_utils.h"

#include "build/build_config.h"
#include "cc/surfaces/surface_id_allocator.h"

#if defined(OS_ANDROID)
#include "content/browser/renderer_host/compositor_impl_android.h"
#else
#include "content/browser/compositor/image_transport_factory.h"
#include "ui/compositor/compositor.h"
#endif

namespace content {

scoped_ptr<cc::SurfaceIdAllocator> CreateSurfaceIdAllocator() {
#if defined(OS_ANDROID)
  return CompositorImpl::CreateSurfaceIdAllocator();
#else
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  return factory->GetContextFactory()->CreateSurfaceIdAllocator();
#endif
}

cc::SurfaceManager* GetSurfaceManager() {
#if defined(OS_ANDROID)
  return CompositorImpl::GetSurfaceManager();
#else
  ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
  return factory->GetSurfaceManager();
#endif
}

}  // namespace content
