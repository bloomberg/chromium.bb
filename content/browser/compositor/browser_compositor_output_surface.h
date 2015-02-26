// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include "base/threading/non_thread_safe.h"
#include "cc/output/output_surface.h"
#include "content/common/content_export.h"
#include "ui/compositor/compositor_vsync_manager.h"

namespace cc {
class SoftwareOutputDevice;
}

namespace content {
class ContextProviderCommandBuffer;
class ReflectorImpl;
class WebGraphicsContext3DCommandBufferImpl;

class CONTENT_EXPORT BrowserCompositorOutputSurface
    : public cc::OutputSurface,
      public ui::CompositorVSyncManager::Observer {
 public:
  ~BrowserCompositorOutputSurface() override;

  // cc::OutputSurface implementation.
  bool BindToClient(cc::OutputSurfaceClient* client) override;

  // ui::CompositorOutputSurface::Observer implementation.
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override;

  void OnUpdateVSyncParametersFromGpu(base::TimeTicks tiembase,
                                      base::TimeDelta interval);

  void SetReflector(ReflectorImpl* reflector);

#if defined(OS_MACOSX)
  virtual void OnSurfaceDisplayed() = 0;
  virtual void OnSurfaceRecycled() = 0;
  virtual bool ShouldNotShowFramesAfterRecycle() const = 0;
#endif

 protected:
  // Constructor used by the accelerated implementation.
  BrowserCompositorOutputSurface(
      const scoped_refptr<cc::ContextProvider>& context,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager);

  // Constructor used by the software implementation.
  BrowserCompositorOutputSurface(
      scoped_ptr<cc::SoftwareOutputDevice> software_device,
      const scoped_refptr<ui::CompositorVSyncManager>& vsync_manager);

  scoped_refptr<ui::CompositorVSyncManager> vsync_manager_;
  ReflectorImpl* reflector_;

 private:
  void Initialize();

  DISALLOW_COPY_AND_ASSIGN(BrowserCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
