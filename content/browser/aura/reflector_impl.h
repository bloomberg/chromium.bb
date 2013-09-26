// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AURA_REFLECTOR_IMPL_H_
#define CONTENT_BROWSER_AURA_REFLECTOR_IMPL_H_

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/aura/image_transport_factory.h"
#include "ui/compositor/reflector.h"
#include "ui/gfx/size.h"

namespace base { class MessageLoopProxy; }

namespace gfx { class Rect; }

namespace ui {
class Compositor;
class Layer;
}

namespace content {

class BrowserCompositorOutputSurface;

// A reflector implementation that copies the framebuffer content
// to the texture, then draw it onto the mirroring compositor.
class ReflectorImpl : public ImageTransportFactoryObserver,
                      public base::SupportsWeakPtr<ReflectorImpl>,
                      public ui::Reflector {
 public:
  ReflectorImpl(
      ui::Compositor* mirrored_compositor,
      ui::Layer* mirroring_layer,
      IDMap<BrowserCompositorOutputSurface>* output_surface_map,
      int surface_id);

  ui::Compositor* mirrored_compositor() {
    return mirrored_compositor_;
  }

  void InitOnImplThread();
  void Shutdown();
  void ShutdownOnImplThread();

  // This must be called on ImplThread, or before the surface is passed to
  // ImplThread.
  void AttachToOutputSurface(BrowserCompositorOutputSurface* surface);

  // ui::Reflector implementation.
  virtual void OnMirroringCompositorResized() OVERRIDE;

  // ImageTransportFactoryObsever implementation.
  virtual void OnLostResources() OVERRIDE;

  // Called when the output surface's size has changed.
  // This must be called on ImplThread.
  void OnReshape(gfx::Size size);

  // Called in |BrowserCompositorOutputSurface::SwapBuffers| to copy
  // the full screen image to the |texture_id_|. This must be called
  // on ImplThread.
  void OnSwapBuffers();

  // Called in |BrowserCompositorOutputSurface::PostSubBuffer| copy
  // the sub image given by |rect| to the texture.This must be called
  // on ImplThread.
  void OnPostSubBuffer(gfx::Rect rect);

  // Create a shared texture that will be used to copy the content of
  // mirrored compositor to the mirroring compositor.  This must be
  // called before the reflector is attached to OutputSurface to avoid
  // race with ImplThread accessing |texture_id_|.
  void CreateSharedTexture();

  // Called when the source surface is bound and available. This must
  // be called on ImplThread.
  void OnSourceSurfaceReady(int surface_id);

 private:
  virtual ~ReflectorImpl();

  void UpdateTextureSizeOnMainThread(gfx::Size size);

  // Request full redraw on mirroring compositor.
  void FullRedrawOnMainThread(gfx::Size size);

  void UpdateSubBufferOnMainThread(gfx::Size size, gfx::Rect rect);

  // Request full redraw on mirrored compositor so that
  // the full content will be copied to mirroring compositor.
  void FullRedrawContentOnMainThread();

  // This exists just to hold a reference to a ReflectorImpl in a post task,
  // so the ReflectorImpl gets deleted when the function returns.
  static void DeleteOnMainThread(scoped_refptr<ReflectorImpl> reflector) {}

  // These variables are initialized on MainThread before
  // the reflector is attached to the output surface. Once
  // attached, they must be accessed only on ImplThraed unless
  // the context is lost. When the context is lost, these
  // will be re-ininitiailzed when the new output-surface
  // is created on MainThread.
  int texture_id_;
  gfx::Size texture_size_;

  // Must be accessed only on ImplThread.
  IDMap<BrowserCompositorOutputSurface>* output_surface_map_;
  scoped_ptr<GLHelper> gl_helper_;

  // Must be accessed only on MainThread.
  ui::Compositor* mirrored_compositor_;
  ui::Compositor* mirroring_compositor_;
  ui::Layer* mirroring_layer_;
  scoped_refptr<ui::Texture> shared_texture_;
  scoped_refptr<base::MessageLoopProxy> impl_message_loop_;
  scoped_refptr<base::MessageLoopProxy> main_message_loop_;
  int surface_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AURA_REFLECTOR_IMPL_H_
