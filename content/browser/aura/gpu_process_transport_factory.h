// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AURA_GPU_PROCESS_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_AURA_GPU_PROCESS_TRANSPORT_FACTORY_H_

#include <map>

#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/browser/aura/image_transport_factory.h"
#include "ui/compositor/compositor.h"

namespace content {
class BrowserCompositorOutputSurface;
class BrowserCompositorOutputSurfaceProxy;
class CompositorSwapClient;
class ContextProviderCommandBuffer;
class ReflectorImpl;
class WebGraphicsContext3DCommandBufferImpl;
class WebGraphicsContext3DSwapBuffersClient;

class GpuProcessTransportFactory
    : public ui::ContextFactory,
      public ImageTransportFactory {
 public:
  GpuProcessTransportFactory();

  virtual ~GpuProcessTransportFactory();

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl>
  CreateOffscreenCommandBufferContext();

  // ContextFactory implementation.
  virtual scoped_ptr<WebKit::WebGraphicsContext3D> CreateOffscreenContext()
      OVERRIDE;
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(
      ui::Compositor* compositor) OVERRIDE;
  virtual scoped_refptr<ui::Reflector> CreateReflector(
      ui::Compositor* source,
      ui::Layer* target) OVERRIDE;
  virtual void RemoveReflector(
      scoped_refptr<ui::Reflector> reflector) OVERRIDE;
  virtual void RemoveCompositor(ui::Compositor* compositor) OVERRIDE;
  virtual bool DoesCreateTestContexts() OVERRIDE;

  // ImageTransportFactory implementation.
  virtual ui::ContextFactory* AsContextFactory() OVERRIDE;
  virtual gfx::GLSurfaceHandle CreateSharedSurfaceHandle() OVERRIDE;
  virtual void DestroySharedSurfaceHandle(
      gfx::GLSurfaceHandle surface) OVERRIDE;
  virtual scoped_refptr<ui::Texture> CreateTransportClient(
      float device_scale_factor) OVERRIDE;
  virtual scoped_refptr<ui::Texture> CreateOwnedTexture(
      const gfx::Size& size,
      float device_scale_factor,
      unsigned int texture_id) OVERRIDE;
  virtual GLHelper* GetGLHelper() OVERRIDE;
  virtual uint32 InsertSyncPoint() OVERRIDE;
  virtual void WaitSyncPoint(uint32 sync_point) OVERRIDE;
  virtual void AddObserver(ImageTransportFactoryObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      ImageTransportFactoryObserver* observer) OVERRIDE;

  // ui::ContextFactory implementation.
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForMainThread() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForCompositorThread() OVERRIDE;

  void OnLostContext(ui::Compositor* compositor);

 private:
  struct PerCompositorData;

  PerCompositorData* CreatePerCompositorData(ui::Compositor* compositor);
  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> CreateContextCommon(
      const base::WeakPtr<WebGraphicsContext3DSwapBuffersClient>& swap_client,
      int surface_id);

  void CreateSharedContextLazy();

  void OnLostMainThreadSharedContextInsideCallback();
  void OnLostMainThreadSharedContext();

  typedef std::map<ui::Compositor*, PerCompositorData*> PerCompositorDataMap;
  PerCompositorDataMap per_compositor_data_;
  scoped_refptr<ContextProviderCommandBuffer> shared_contexts_main_thread_;
  scoped_refptr<ContextProviderCommandBuffer>
      shared_contexts_compositor_thread_;
  scoped_ptr<GLHelper> gl_helper_;
  ObserverList<ImageTransportFactoryObserver> observer_list_;
  base::WeakPtrFactory<GpuProcessTransportFactory> callback_factory_;

  // The contents of this map and its methods may only be used on the compositor
  // thread.
  IDMap<BrowserCompositorOutputSurface> output_surface_map_;

  scoped_refptr<BrowserCompositorOutputSurfaceProxy> output_surface_proxy_;

  DISALLOW_COPY_AND_ASSIGN(GpuProcessTransportFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_AURA_GPU_PROCESS_TRANSPORT_FACTORY_H_
