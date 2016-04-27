// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_TEST_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_COMPOSITOR_TEST_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/browser/compositor/image_transport_factory.h"

namespace cc {
class ContextProvider;
}

namespace content {

// An ImageTransportFactory that disables transport.
class NoTransportImageTransportFactory : public ImageTransportFactory {
 public:
  NoTransportImageTransportFactory();
  ~NoTransportImageTransportFactory() override;

  // ImageTransportFactory implementation.
  ui::ContextFactory* GetContextFactory() override;
  cc::SurfaceManager* GetSurfaceManager() override;
  display_compositor::GLHelper* GetGLHelper() override;
  void AddObserver(ImageTransportFactoryObserver* observer) override;
  void RemoveObserver(ImageTransportFactoryObserver* observer) override;
#if defined(OS_MACOSX)
  void OnGpuSwapBuffersCompleted(
      gpu::SurfaceHandle surface_handle,
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result) override {}
  void SetCompositorSuspendedForRecycle(ui::Compositor* compositor,
                                        bool suspended) override {}
  bool SurfaceShouldNotShowFramesAfterSuspendForRecycle(
      gpu::SurfaceHandle surface_handle) const override;
#endif

 private:
  std::unique_ptr<cc::SurfaceManager> surface_manager_;
  std::unique_ptr<ui::ContextFactory> context_factory_;
  scoped_refptr<cc::ContextProvider> context_provider_;
  std::unique_ptr<display_compositor::GLHelper> gl_helper_;
  base::ObserverList<ImageTransportFactoryObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(NoTransportImageTransportFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_TEST_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_H_
